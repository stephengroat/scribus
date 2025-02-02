/*
 For general Scribus (>=1.3.2) copyright and licensing information please refer
 to the COPYING file provided with the program. Following this notice may exist
 a copyright and/or license notice that predates the release of Scribus 1.3.2
 for which a new license (GPL+exception) is in place.
 */
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/


#include "canvasmode_create.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCursor>
#include <QEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPoint>
#include <QRect>
#include <QWidgetAction>

#include "appmodes.h"
#include "canvas.h"
#include "fpoint.h"
#include "fpointarray.h"
#include "hyphenator.h"
#include "pageitem_table.h"
#include "pageitem_textframe.h"
#include "prefscontext.h"
#include "prefsfile.h"
#include "prefsmanager.h"
#include "scraction.h"
#include "scribus.h"
#include "scribusXml.h"
#include "scribusdoc.h"
#include "scribusview.h"
#include "selection.h"
#include "text/frect.h"
#include "ui/basepointwidget.h"
#include "ui/hruler.h"
#include "ui/insertTable.h"
#include "ui/pageselector.h"
#include "ui/propertiespalette.h"
#include "ui/scrapbookpalette.h"
#include "ui/vruler.h"
#include "undomanager.h"
#include "units.h"
#include "util.h"
#include "util_math.h"

CreateMode::CreateMode(ScribusView* view) : CanvasMode(view) 
{
	canvasPressCoord.setXY(-1.0, -1.0);
	mouseGlobalCoord.setXY(-1.0, -1.0);
}

void CreateMode::drawControls(QPainter* p) 
{
	if (!inItemCreation) return;

	QPointF topLeft(createObjectPos.x(), createObjectPos.y());
	QPointF btRight(canvasCurrCoord.x(), canvasCurrCoord.y());
	QColor  drawColor = qApp->palette().color(QPalette::Active, QPalette::Highlight);

	if (createObjectMode != modeDrawLine)
	{
		QRectF bounds = QRectF(topLeft, btRight).normalized();
		//Lock Height to Width for Control Modifier for region drawing
		if (modifiers==Qt::ControlModifier)
		{
			bounds.setHeight(bounds.width());
			if (btRight.y()<topLeft.y())
				bounds.moveBottom(topLeft.y());
			if (btRight.x()<topLeft.x() && btRight.y()>topLeft.y())
				bounds.moveTop(topLeft.y());
		}
		QRect localRect = m_canvas->canvasToLocal(bounds);
		if (localRect.width() <= 0 || localRect.height() <= 0)
			return;
		p->setRenderHint(QPainter::Antialiasing);

		p->save();
		p->setPen(QPen(drawColor, 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
		drawColor.setAlpha(64);
		p->setBrush(drawColor);
		p->drawRect(localRect);

		drawColor.setAlpha(255);
		p->setBrush(Qt::NoBrush);
		p->setPen(QPen(drawColor, 1, Qt::DashLine, Qt::FlatCap, Qt::MiterJoin));

		int frameType = 0, itemType = 0;
		getFrameItemTypes(itemType, frameType);
		if (frameType == PageItem::Ellipse)
		{
			p->drawEllipse(localRect);
		}
		else if (createObjectMode == modeDrawArc)
		{
			QPainterPath path;
			path.moveTo(localRect.width() / 2.0, localRect.height() / 2.0);
			path.arcTo(0.0, 0.0, localRect.width(), localRect.height(), m_doc->itemToolPrefs().arcStartAngle, m_doc->itemToolPrefs().arcSweepAngle);
			path.closeSubpath();
			p->translate(localRect.left(), localRect.top());
			p->drawPath(path);
		}
		else if (createObjectMode == modeDrawRegularPolygon)
		{
			QPainterPath path = regularPolygonPath(localRect.width(), localRect.height(), m_doc->itemToolPrefs().polyCorners, m_doc->itemToolPrefs().polyUseFactor, m_doc->itemToolPrefs().polyFactor, m_doc->itemToolPrefs().polyRotation, m_doc->itemToolPrefs().polyCurvature, m_doc->itemToolPrefs().polyInnerRot, m_doc->itemToolPrefs().polyOuterCurvature);
			p->translate(localRect.left(), localRect.top());
			p->drawPath(path);
		}
		else if (createObjectMode == modeDrawSpiral)
		{
			QPainterPath path = spiralPath(localRect.width(), localRect.height(), m_doc->itemToolPrefs().spiralStartAngle, m_doc->itemToolPrefs().spiralEndAngle, m_doc->itemToolPrefs().spiralFactor);
			p->translate(localRect.left(), localRect.top());
			p->drawPath(path);
		}
		else if ((createObjectMode == modeDrawShapes) && (createObjectSubMode > 1))
		{
			FPointArray poly;
			int valCount = m_doc->ValCount;
			double *vals = m_doc->ShapeValues;
			for (int a = 0; a < valCount-3; a += 4)
			{
				if (vals[a] < 0)
				{
					poly.setMarker();
					continue;
				}
				double x1 = localRect.width()  * vals[a] / 100.0;
				double y1 = localRect.height() * vals[a+1] / 100.0;
				double x2 = localRect.width()  * vals[a+2] / 100.0;
				double y2 = localRect.height() * vals[a+3] / 100.0;
				poly.addPoint(x1, y1);
				poly.addPoint(x2, y2);
			}
			QPainterPath path = poly.toQPainterPath(false);
			p->translate(localRect.left(), localRect.top());
			p->drawPath(path);
		}
		p->restore();
	}
	else
	{
		QPoint p1 = m_canvas->canvasToLocal(topLeft);
		QPoint p2 = m_canvas->canvasToLocal(btRight);
		
		p->save();
		p->setPen(QPen(drawColor, 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
		p->setBrush(drawColor);
		p->drawLine(p1, p2);
		p->restore();
	}
}

inline bool CreateMode::GetItem(PageItem** pi)
{ 
	*pi = m_doc->m_Selection->itemAt(0); 
	return (*pi) != nullptr;
}

// the following code was moved from scribusview.cpp:


void CreateMode::enterEvent(QEvent *e)
{
	if (!m_MouseButtonPressed)
	{
		setModeCursor();
	}
}


void CreateMode::leaveEvent(QEvent *e)
{
}


void CreateMode::activate(bool fromGesture)
{
//	qDebug() << "CreateMode::activate" << fromGesture;
	CanvasMode::activate(fromGesture);

	PageItem* currItem;
	if (!fromGesture || !GetItem(&currItem) || !m_createTransaction)
	{
		if (m_createTransaction)
		{
//			qDebug() << "canceling left over create Transaction";
			m_createTransaction.cancel();
			m_createTransaction.reset();
		}
		canvasPressCoord.setXY(-1.0, -1.0);
		mouseGlobalCoord.setXY(-1.0, -1.0);
		inItemCreation = false;
	}		
	setModeCursor();
}

void CreateMode::deactivate(bool forGesture)
{
//	qDebug() << "CreateMode::deactivate" << forGesture;
	if (!forGesture)
	{		
		if (m_createTransaction)
		{
//			qDebug() << "CreateMode::deactivate: canceling left over create Transaction";
			m_createTransaction.cancel();
			m_createTransaction.reset();
		}
	}

	CanvasMode::deactivate(forGesture);
}

void CreateMode::keyPressEvent(QKeyEvent *e)
{
	commonkeyPressEvent_Default(e);
}

void CreateMode::keyReleaseEvent(QKeyEvent *e)
{
	commonkeyReleaseEvent(e);
}

void CreateMode::mouseDoubleClickEvent(QMouseEvent *m)
{
	m->accept();
	m_MouseButtonPressed = false;
	m_canvas->resetRenderMode();
}

void CreateMode::mouseMoveEvent(QMouseEvent *m)
{
	const FPoint mousePointDoc = m_canvas->globalToCanvas(m->globalPos());
	modifiers = m->modifiers();
	
	double newX, newY;
	PageItem *currItem;
	QPainter p;
//	QRect tx;
	m->accept();
//	qDebug() << "legacy mode move:" << m->x() << m->y() << m_canvas->globalToCanvas(m->globalPos()).x() << m_canvas->globalToCanvas(m->globalPos()).y();
//	emit MousePos(m->x()/m_canvas->scale(),// + m_doc->minCanvasCoordinate.x(), 
//				  m->y()/m_canvas->scale()); // + m_doc->minCanvasCoordinate.y());

	if (commonMouseMove(m))
		return;
	if (GetItem(&currItem))
	{
//		newX = mousePointDoc.x(); //m_view->translateToDoc(m->x(), m->y()).x());
//		newY = mousePointDoc.y(); //m_view->translateToDoc(m->x(), m->y()).y());
		return;
	}
	if ((!m_MouseButtonPressed) || !(m->buttons() & Qt::LeftButton))
	{
		m_canvas->displayCorrectedXYHUD(m->globalPos(), mousePointDoc.x(), mousePointDoc.y());
		return;
	}

	newX = mousePointDoc.x();
	newY = mousePointDoc.y();
	if (createObjectMode == modeDrawLine)
	{
		if (m_doc->SnapGrid)
		{
			newX = qRound(newX / m_doc->guidesPrefs().minorGridSpacing) * m_doc->guidesPrefs().minorGridSpacing;
			newY = qRound(newY / m_doc->guidesPrefs().minorGridSpacing) * m_doc->guidesPrefs().minorGridSpacing;
		}
		if (m->modifiers() & Qt::ControlModifier)
		{
			QRectF bounds(QPointF(createObjectPos.x(), createObjectPos.y()), QPointF(newX, newY));
			double newRot = xy2Deg(bounds.width(), bounds.height());
			if (newRot < 0.0)
				newRot += 360;
			newRot = constrainAngle(newRot, m_doc->opToolPrefs().constrain);
			double len = qMax(0.01, distance(bounds.width(), bounds.height()));
			bounds.setSize(len * QSizeF(cosd(newRot), sind(newRot)));
			newX = bounds.right();
			newY = bounds.bottom();
		}
	}
	//CB: #8099: Readd snapping for drag creation of lines by commenting this else..
	//else
	//{
		FPoint np2 = m_doc->ApplyGridF(FPoint(newX, newY));
		double nx = np2.x();
		double ny = np2.y();
		m_doc->ApplyGuides(&nx, &ny);
		m_doc->ApplyGuides(&nx, &ny,true);
		if (nx != np2.x())
			xSnap = nx;
		if (ny != np2.y())
			ySnap = ny;
		// #8959 : suppress qRound here as this prevent drawing line with angle constrain
		// precisely and does not allow to stick precisely to grid or guides
		newX = /*qRound(*/nx/*)*/;
		newY = /*qRound(*/ny/*)*/;
	//}

	canvasCurrCoord.setXY(newX, newY);
	m_view->HaveSelRect = true;

	double wSize = canvasCurrCoord.x() - createObjectPos.x();
	double hSize = canvasCurrCoord.y() - createObjectPos.y();
	QRectF createObjectRect(createObjectPos.x(), createObjectPos.y(), wSize, hSize);
	createObjectRect = createObjectRect.normalized();
	if (createObjectMode != modeDrawLine)
	{
		if (modifiers == Qt::ControlModifier)
			hSize = wSize;
		m_canvas->displaySizeHUD(m->globalPos(), wSize, hSize, false);
	}
	else
	{
		double angle = -xy2Deg(wSize, hSize);
		if (angle < 0.0)
			angle = angle + 360;
		double trueLength = sqrt(pow(createObjectRect.width(), 2) + pow(createObjectRect.height(), 2));
		m_canvas->displaySizeHUD(m->globalPos(), trueLength, angle, true);
	}

	// Necessary for drawControls to be called
	m_canvas->repaint();
}

void CreateMode::mousePressEvent(QMouseEvent *m)
{
	const FPoint mousePointDoc = m_canvas->globalToCanvas(m->globalPos());
	modifiers = m->modifiers();
	
	double Rxp = 0, Ryp = 0;
	m_MouseButtonPressed = true;
	m_view->HaveSelRect = false;
	m_doc->DragP = false;
	m_doc->leaveDrag = false;
	inItemCreation = false;
//	oldClip = 0;
	m->accept();
	m_view->registerMousePress(m->globalPos());
//	QRect mpo(m->x()-m_doc->guidesPrefs().grabRadius, m->y()-m_doc->guidesPrefs().grabRadius, m_doc->guidesPrefs().grabRadius*2, m_doc->guidesPrefs().grabRadius*2);
//	mpo.moveBy(qRound(m_doc->minCanvasCoordinate.x() * m_canvas->scale()), qRound(m_doc->minCanvasCoordinate.y() * m_canvas->scale()));
	canvasPressCoord = mousePointDoc;
	createObjectMode = m_doc->appMode;
	createObjectSubMode = m_doc->SubMode;
	createObjectPos  = m_doc->ApplyGridF(canvasPressCoord);
	
	Rxp  = m_doc->ApplyGridF(canvasPressCoord).x();
	canvasPressCoord.setX(qRound(Rxp));
	Ryp  = m_doc->ApplyGridF(canvasPressCoord).y();
	canvasPressCoord.setXY(qRound(Rxp), qRound(Ryp));

	canvasCurrCoord = canvasPressCoord;
	m_doc->ApplyGuides(&Rxp, &Ryp);
	m_doc->ApplyGuides(&Rxp, &Ryp,true);
	createObjectPos.setXY(Rxp, Ryp);

	if (m->button() == Qt::MiddleButton)
	{
		m_view->MidButt = true;
		if (m->modifiers() & Qt::ControlModifier)
			m_view->DrawNew();
		return;
	}
	
	if (m->button() == Qt::RightButton)
	{
		m_view->requestMode(modeNormal);
		return;
	}
	if (m->button() != Qt::LeftButton)
		return;
	selectPage(m);

	switch (m_doc->appMode)
	{
		case modeDrawTable2:
			m_view->deselectItems(false);
			break;
	}

	inItemCreation = true;
}

void CreateMode::mouseReleaseEvent(QMouseEvent *m)
{
	modifiers = m->modifiers();

	PageItem *currItem;
	m_MouseButtonPressed = false;
	m_canvas->resetRenderMode();
	m->accept();
//	m_view->stopDragTimer();

	m_createTransaction = Um::instance()->beginTransaction("creating");
	currItem = doCreateNewObject();
	if (m_createTransaction && currItem)
	{
		m_view->resetMousePressed();
		currItem->checkChanges(true);
		QString targetName = Um::ScratchSpace;
		if (currItem->OwnPage > -1)
			targetName = m_doc->Pages->at(currItem->OwnPage)->getUName();
		m_createTransaction.commit(targetName, currItem->getUPixmap(),
									Um::Create + " " + currItem->getUName(),  "", Um::ICreate);
		m_createTransaction.reset();
		m_doc->changed();
		/*currItem->update();
		currItem->emitAllToGUI();*/
	}
	else if (m_createTransaction)
	{
		m_createTransaction.cancel();
		m_createTransaction.reset();
	}
	if (!PrefsManager::instance().appPrefs.uiPrefs.stickyTools)
	{
		m_view->requestMode(modeNormal);
	}
	else
	{
		m_view->updateCanvas();
		int appMode = m_doc->appMode;
		m_view->requestMode(appMode);
	}
	inItemCreation = false;
}

void CreateMode::selectPage(QMouseEvent *m)
{
	m_MouseButtonPressed = true;
	FPoint mousePointDoc = m_canvas->globalToCanvas(m->globalPos());
	canvasPressCoord     = mousePointDoc;
//	QRect mpo(m->x()-m_doc->guidesPrefs().grabRadius, m->y()-m_doc->guidesPrefs().grabRadius, m_doc->guidesPrefs().grabRadius*2, m_doc->guidesPrefs().grabRadius*2);
//	mpo.moveBy(qRound(Doc->minCanvasCoordinate.x() * m_canvas->scale()), qRound(m_doc->minCanvasCoordinate.y() * m_canvas->scale()));
	m_doc->nodeEdit.deselect();
	m_view->deselectItems(false);
	if (!m_doc->masterPageMode())
	{
		int i = m_doc->OnPage(canvasPressCoord.x(), canvasPressCoord.y());
		if (i!=-1)
		{
			uint docCurrPageNo=m_doc->currentPageNumber();
			uint j=static_cast<uint>(i);
			if (docCurrPageNo != j)
			{
				m_doc->setCurrentPage(m_doc->Pages->at(j));
				m_view->m_ScMW->slotSetCurrentPage(j);
				m_view->DrawNew();
			}
		}
/*		uint docPagesCount=m_doc->Pages->count();
		uint docCurrPageNo=m_doc->currentPageNumber();
		for (uint i = 0; i < docPagesCount; ++i)
		{
			int x = static_cast<int>(m_doc->Pages->at(i)->xOffset() * m_canvas->scale());
			int y = static_cast<int>(m_doc->Pages->at(i)->yOffset() * m_canvas->scale());
			int w = static_cast<int>(m_doc->Pages->at(i)->width() * m_canvas->scale());
			int h = static_cast<int>(m_doc->Pages->at(i)->height() * m_canvas->scale());
			if (QRect(x, y, w, h).intersects(mpo))
			{
				if (docCurrPageNo != i)
				{
					m_doc->setCurrentPage(m_doc->Pages->at(i));
					setMenTxt(i);
					DrawNew();
				}
				break;
			}
		} */

		//FIXME m_view->setRulerPos(m_view->contentsX(), m_view->contentsY());
	}
}

void CreateMode::SetupDrawNoResize(int nr)
{
	PageItem* currItem = m_doc->Items->at(nr);
	//	currItem->setFont(Doc->toolSettings.textFont);
	//	currItem->setFontSize(Doc->toolSettings.textSize);
	m_doc->m_Selection->delaySignalsOn();
	m_doc->m_Selection->clear();
	m_doc->m_Selection->addItem(currItem);
	m_doc->m_Selection->delaySignalsOff();
//	emit DocChanged();
	currItem->Sizing =  false /*currItem->asLine() ? false : true*/;
	//#6456 m_view->resetMoveTimer();
}

void CreateMode::getFrameItemTypes(int& itemType, int& frameType)
{
	itemType  = (int) PageItem::Polygon;
	frameType = (int) PageItem::Rectangle;
	switch (createObjectMode)
	{
	case modeDrawShapes:
		switch (createObjectSubMode)
		{
		case 0:
			itemType  = (int) PageItem::Polygon;
			frameType = (int) PageItem::Rectangle;
			break;
		case 1:
			itemType  = (int) PageItem::Polygon;
			frameType = (int) PageItem::Ellipse;
			break;
		default:
			itemType  = (int) PageItem::Polygon;
			frameType = (int) PageItem::Unspecified;
			break;
		}
		break;
	case modeDrawLine:
		itemType  = (int) PageItem::Line;
		frameType = (int) PageItem::Unspecified;
		break;
	case modeDrawLatex:
		itemType  = (int) PageItem::LatexFrame;
		frameType = (int) PageItem::Unspecified;
		break;
	case modeDrawImage:
		itemType  = (int) PageItem::ImageFrame;
		frameType = (int) PageItem::Unspecified;
		break;
	case modeDrawText:
		itemType  = (int) PageItem::TextFrame;
		frameType = (int) PageItem::Unspecified;
		break;
	case modeDrawRegularPolygon:
		itemType  = (int) PageItem::RegularPolygon;
		frameType = (int) PageItem::Unspecified;
		break;
	case modeDrawArc:
		itemType  = (int) PageItem::Arc;
		frameType = (int) PageItem::Unspecified;
		break;
	case modeDrawSpiral:
		itemType  = (int) PageItem::Spiral;
		frameType = (int) PageItem::Unspecified;
		break;
	case modeInsertPDFButton:
	case modeInsertPDFRadioButton:
	case modeInsertPDFTextfield:
	case modeInsertPDFCheckbox:
	case modeInsertPDFCombobox:
	case modeInsertPDFListbox:
	case modeInsertPDFTextAnnotation:
	case modeInsertPDFLinkAnnotation:
		itemType  = (int) PageItem::TextFrame;
		frameType = (int) PageItem::Unspecified;
		break;
	case modeDrawTable2:
		itemType  = (int) PageItem::Table;
		frameType = (int) PageItem::Unspecified;
		break;
	case modeInsertPDF3DAnnotation:
		itemType  = (int) PageItem::OSGFrame;
		frameType = (int) PageItem::Unspecified;
		break;
	}
}

PageItem* CreateMode::doCreateNewObject()
{
	int z = -1;
	double rot, len;

	double wSize = canvasCurrCoord.x() - createObjectPos.x();
	double hSize = canvasCurrCoord.y() - createObjectPos.y();
	bool   skipOneClick = (modifiers == Qt::ShiftModifier);
	if ((createObjectMode == modeDrawLine) || (createObjectMode == modeDrawTable2) ||
		(createObjectMode == modeInsertPDFButton) || (createObjectMode == modeInsertPDFTextfield) ||
		(createObjectMode == modeInsertPDFTextfield) || (createObjectMode == modeInsertPDFCheckbox) ||
		(createObjectMode == modeInsertPDFCombobox) || (createObjectMode == modeInsertPDFListbox) ||
		(createObjectMode == modeInsertPDFTextAnnotation) || (createObjectMode == modeInsertPDFLinkAnnotation) ||
		(createObjectMode == modeInsertPDF3DAnnotation) || (createObjectMode == modeInsertPDFRadioButton))
	{
		skipOneClick = false;
	}
	if (!skipOneClick)
	{
		if ((!m_view->moveTimerElapsed()) || ((fabs(wSize) < 2.0) && (fabs(hSize) < 2.0)))
		{
			if (!doOneClick(createObjectPos, canvasCurrCoord))
			{
				return nullptr;
			}
		}
	}

	wSize = canvasCurrCoord.x() - createObjectPos.x();
	hSize = canvasCurrCoord.y() - createObjectPos.y();
	//Lock Height to Width for Control Modifier for final item creation
	if (createObjectMode != modeDrawLine)
	{
		if (modifiers == Qt::ControlModifier)
			hSize = wSize;
	}

	PageItem *newObject = nullptr, *currItem = nullptr;
	// FIXME for modeDrawLine
	QRectF createObjectRect(createObjectPos.x(), createObjectPos.y(), wSize, hSize);
	if (createObjectMode != modeDrawLine)
	{
		createObjectRect = createObjectRect.normalized();
		if (modifiers==Qt::ControlModifier)
		{
			//bottom right and upper left are ok
			//upper right
			if (canvasCurrCoord.y() < createObjectPos.y() && createObjectPos.x()<canvasCurrCoord.x())
				createObjectRect.translate(0.0, -createObjectRect.height());
			//bottom left
			if (canvasCurrCoord.x()<createObjectPos.x() && canvasCurrCoord.y()>createObjectPos.y())
				createObjectRect.translate(0.0, createObjectRect.height());
		}
	}
	double Rxp  = createObjectRect.x();
	double Ryp  = createObjectRect.y();
	double Rxpd = createObjectRect.width();
	double Rypd = createObjectRect.height();

	switch (createObjectMode)
	{
	case modeDrawShapes:
		switch (createObjectSubMode)
		{
			case 0:
				if (modifiers == Qt::ShiftModifier)
					z = m_doc->itemAddArea(PageItem::Polygon, PageItem::Rectangle, Rxp, Ryp, m_doc->itemToolPrefs().shapeLineWidth, m_doc->itemToolPrefs().shapeFillColor, m_doc->itemToolPrefs().shapeLineColor);
				else
					z = m_doc->itemAdd(PageItem::Polygon, PageItem::Rectangle, Rxp, Ryp, Rxpd, Rypd, m_doc->itemToolPrefs().shapeLineWidth, m_doc->itemToolPrefs().shapeFillColor, m_doc->itemToolPrefs().shapeLineColor);
				m_doc->Items->at(z)->FrameType = 0;
				break;
			case 1:
				if (modifiers == Qt::ShiftModifier)
					z = m_doc->itemAddArea(PageItem::Polygon, PageItem::Ellipse, Rxp, Ryp, m_doc->itemToolPrefs().shapeLineWidth, m_doc->itemToolPrefs().shapeFillColor, m_doc->itemToolPrefs().shapeLineColor);
				else
					z = m_doc->itemAdd(PageItem::Polygon, PageItem::Ellipse, Rxp, Ryp, Rxpd, Rypd, m_doc->itemToolPrefs().shapeLineWidth, m_doc->itemToolPrefs().shapeFillColor, m_doc->itemToolPrefs().shapeLineColor);
				m_doc->Items->at(z)->FrameType = 1;
				break;
			default:
				if (modifiers == Qt::ShiftModifier)
					z = m_doc->itemAddArea(PageItem::Polygon, PageItem::Unspecified, Rxp, Ryp, m_doc->itemToolPrefs().shapeLineWidth, m_doc->itemToolPrefs().shapeFillColor, m_doc->itemToolPrefs().shapeLineColor);
				else
					z = m_doc->itemAdd(PageItem::Polygon, PageItem::Unspecified, Rxp, Ryp, Rxpd, Rypd, m_doc->itemToolPrefs().shapeLineWidth, m_doc->itemToolPrefs().shapeFillColor, m_doc->itemToolPrefs().shapeLineColor);
				m_doc->Items->at(z)->SetFrameShape(m_doc->ValCount, m_doc->ShapeValues);
				m_doc->adjustItemSize(m_doc->Items->at(z));
				m_doc->setRedrawBounding(m_doc->Items->at(z));
				m_doc->Items->at(z)->FrameType = createObjectSubMode + 2;
				break;
		}
		break;
	case modeDrawLine:
		Rxp = createObjectPos.x();
		Ryp = createObjectPos.y();
		m_doc->ApplyGuides(&Rxp, &Ryp);
		m_doc->ApplyGuides(&Rxp, &Ryp,true);
		rot = xy2Deg(Rxpd, Rypd);
		if (rot < 0.0) 
			rot += 360;
		len = qMax(0.01, distance(Rxpd, Rypd));
		z = m_doc->itemAdd(PageItem::Line, PageItem::Unspecified, Rxp, Ryp, len, 1, m_doc->itemToolPrefs().lineWidth, CommonStrings::None, m_doc->itemToolPrefs().lineColor);
		m_doc->Items->at(z)->setRotation(rot);
		m_doc->Items->at(z)->setRedrawBounding();
		break;
	case modeDrawLatex:
		if (modifiers == Qt::ShiftModifier)
			z = m_doc->itemAddArea(PageItem::LatexFrame, PageItem::Unspecified, Rxp, Ryp, 1, m_doc->itemToolPrefs().imageFillColor, m_doc->itemToolPrefs().imageStrokeColor);
		else
			z = m_doc->itemAdd(PageItem::LatexFrame, PageItem::Unspecified, Rxp, Ryp, Rxpd, Rypd, m_doc->itemToolPrefs().shapeLineWidth, m_doc->itemToolPrefs().imageFillColor, m_doc->itemToolPrefs().imageStrokeColor);
		break;
	case modeDrawImage:
		if (modifiers == Qt::ShiftModifier)
			z = m_doc->itemAddArea(PageItem::ImageFrame, PageItem::Unspecified, Rxp, Ryp, 1, m_doc->itemToolPrefs().imageFillColor, m_doc->itemToolPrefs().imageStrokeColor);
		else
			z = m_doc->itemAdd(PageItem::ImageFrame, PageItem::Unspecified, Rxp, Ryp, Rxpd, Rypd, m_doc->itemToolPrefs().shapeLineWidth, m_doc->itemToolPrefs().imageFillColor, m_doc->itemToolPrefs().imageStrokeColor);
		break;
	case modeDrawText:
		if (modifiers == Qt::ShiftModifier)
			z = m_doc->itemAddArea(PageItem::TextFrame, PageItem::Unspecified, Rxp, Ryp, m_doc->itemToolPrefs().shapeLineWidth, CommonStrings::None, m_doc->itemToolPrefs().textFont);
		else
			z = m_doc->itemAdd(PageItem::TextFrame, PageItem::Unspecified, Rxp, Ryp, Rxpd, Rypd, m_doc->itemToolPrefs().shapeLineWidth, CommonStrings::None, m_doc->itemToolPrefs().textFont);
		break;
	case modeDrawRegularPolygon:
		if (modifiers == Qt::ShiftModifier)
			z = m_doc->itemAddArea(PageItem::RegularPolygon, PageItem::Unspecified, Rxp, Ryp, m_doc->itemToolPrefs().shapeLineWidth, m_doc->itemToolPrefs().shapeFillColor, m_doc->itemToolPrefs().lineColor);
		else
			z = m_doc->itemAdd(PageItem::RegularPolygon, PageItem::Unspecified, Rxp, Ryp, Rxpd, Rypd, m_doc->itemToolPrefs().shapeLineWidth, m_doc->itemToolPrefs().shapeFillColor, m_doc->itemToolPrefs().lineColor);
		break;
	case modeDrawArc:
		if (modifiers == Qt::ShiftModifier)
			z = m_doc->itemAddArea(PageItem::Arc, PageItem::Unspecified, Rxp, Ryp, m_doc->itemToolPrefs().shapeLineWidth, m_doc->itemToolPrefs().shapeFillColor, m_doc->itemToolPrefs().lineColor);
		else
			z = m_doc->itemAdd(PageItem::Arc, PageItem::Unspecified, Rxp, Ryp, Rxpd, Rypd, m_doc->itemToolPrefs().shapeLineWidth, m_doc->itemToolPrefs().shapeFillColor, m_doc->itemToolPrefs().lineColor);
		m_doc->setRedrawBounding(m_doc->Items->at(z));
		break;
	case modeDrawSpiral:
		if (modifiers == Qt::ShiftModifier)
			z = m_doc->itemAddArea(PageItem::Spiral, PageItem::Unspecified, Rxp, Ryp, m_doc->itemToolPrefs().shapeLineWidth, CommonStrings::None, m_doc->itemToolPrefs().lineColor);
		else
			z = m_doc->itemAdd(PageItem::Spiral, PageItem::Unspecified, Rxp, Ryp, Rxpd, Rypd, m_doc->itemToolPrefs().shapeLineWidth, CommonStrings::None, m_doc->itemToolPrefs().lineColor);
		m_doc->adjustItemSize(m_doc->Items->at(z));
		m_doc->setRedrawBounding(m_doc->Items->at(z));
		break;
	case modeInsertPDFButton:
	case modeInsertPDFRadioButton:
	case modeInsertPDFTextfield:
	case modeInsertPDFCheckbox:
	case modeInsertPDFCombobox:
	case modeInsertPDFListbox:
	case modeInsertPDFTextAnnotation:
	case modeInsertPDFLinkAnnotation:
		z = m_doc->itemAdd(PageItem::TextFrame, PageItem::Unspecified, Rxp, Ryp, Rxpd, Rypd, m_doc->itemToolPrefs().shapeLineWidth, CommonStrings::None, m_doc->itemToolPrefs().textColor);
		currItem = m_doc->Items->at(z);
		currItem->setIsAnnotation(true);
		currItem->AutoName = false;
		switch (m_doc->appMode)
		{
		case modeInsertPDFButton:
			currItem->annotation().setType(Annotation::Button);
			currItem->annotation().setFlag(Annotation::Flag_PushButton);
			currItem->setItemName( CommonStrings::itemName_PushButton + QString("%1").arg(m_doc->TotalItems));
			break;
		case modeInsertPDFRadioButton:
			currItem->annotation().setType(Annotation::RadioButton);
			currItem->annotation().setFlag(Annotation::Flag_Radio | Annotation::Flag_NoToggleToOff);
			currItem->setItemName( CommonStrings::itemName_RadioButton + QString("%1").arg(m_doc->TotalItems));
			break;
		case modeInsertPDFTextfield:
			currItem->annotation().setType(Annotation::Textfield);
			currItem->setItemName( CommonStrings::itemName_TextField + QString("%1").arg(m_doc->TotalItems));
			break;
		case modeInsertPDFCheckbox:
			currItem->annotation().setType(Annotation::Checkbox);
			currItem->setItemName( CommonStrings::itemName_CheckBox + QString("%1").arg(m_doc->TotalItems));
			break;
		case modeInsertPDFCombobox:
			currItem->annotation().setType(Annotation::Combobox);
			currItem->annotation().setFlag(Annotation::Flag_Combo);
			currItem->setItemName( CommonStrings::itemName_ComboBox + QString("%1").arg(m_doc->TotalItems));
			break;
		case modeInsertPDFListbox:
			currItem->annotation().setType(Annotation::Listbox);
			currItem->setItemName( CommonStrings::itemName_ListBox + QString("%1").arg(m_doc->TotalItems));
			break;
		case modeInsertPDFTextAnnotation:
			currItem->annotation().setType(Annotation::Text);
			currItem->setItemName( CommonStrings::itemName_TextAnnotation + QString("%1").arg(m_doc->TotalItems));
			break;
		case modeInsertPDFLinkAnnotation:
			currItem->annotation().setType(Annotation::Link);
			currItem->annotation().setZiel(m_doc->currentPage()->pageNr());
			currItem->annotation().setAction("0 0");
			currItem->setItemName( CommonStrings::itemName_LinkAnnotation + QString("%1").arg(m_doc->TotalItems));
			currItem->setTextFlowMode(PageItem::TextFlowDisabled);
			break;
		}
		break;
	case modeDrawTable2:
		// TODO: Figure out what these conditions actually do.
		if ((m_doc->m_Selection->isEmpty()) && (m_view->HaveSelRect) && (!m_view->MidButt))
		{
			m_view->HaveSelRect = false;
			// Calculate table rectangle.
			FRect tableRect = adjustedRect(canvasPressCoord, canvasCurrCoord);
			if (tableRect.width() < 6 || tableRect.height() < 6)
			{
				// Ignore tiny tables.
				m_view->requestMode(submodePaintingDone);
				break;
			}
			// Show table insert dialog.
		//	qApp->changeOverrideCursor(QCursor(Qt::ArrowCursor));
			InsertTable *dia = new InsertTable(m_view, static_cast<int>(tableRect.height()/6), static_cast<int>(tableRect.width()/6));
			if (!dia->exec())
			{
				m_view->requestMode(submodePaintingDone);
				delete dia;
				dia = nullptr;
				break;
			}
			int numRows = dia->Rows->value();
			int numColumns = dia->Cols->value();
			delete dia;
			dia = nullptr;
			// Add the table item.
			// TODO: This should be done in an undo transaction.
			m_doc->dontResize = true;
			z = m_doc->itemAdd(PageItem::Table, PageItem::Unspecified,
						   tableRect.x(),
						   tableRect.y(),
						   tableRect.width(),
						   tableRect.height(),
						   0,                    // Unused.
						   CommonStrings::None,  // Unused.
						   CommonStrings::None); // Unused.
			PageItem_Table *table = m_doc->Items->at(z)->asTable();
			table->insertRows(0, numRows - 1);
			table->insertColumns(0, numColumns - 1);
			table->adjustTableToFrame();
			table->adjustFrameToTable();
			m_doc->dontResize = false;
			m_doc->setRedrawBounding(table);
		}
		break;
	case modeInsertPDF3DAnnotation:
		if (modifiers == Qt::ShiftModifier)
		{
			z = m_doc->itemAddArea(PageItem::OSGFrame, PageItem::Unspecified, Rxp, Ryp, 1, m_doc->itemToolPrefs().imageFillColor, m_doc->itemToolPrefs().imageStrokeColor);
		}
		else
		{
			z = m_doc->itemAdd(PageItem::OSGFrame, PageItem::Unspecified, Rxp, Ryp, Rxpd, Rypd, m_doc->itemToolPrefs().shapeLineWidth, m_doc->itemToolPrefs().imageFillColor, m_doc->itemToolPrefs().imageStrokeColor);
		}
		currItem = m_doc->Items->at(z);
		currItem->setIsAnnotation(true);
		currItem->AutoName = false;
		currItem->annotation().setType(Annotation::Annot3D);
		currItem->setItemName( tr("3DAnnot") + QString("%1").arg(m_doc->TotalItems));
		break;
	}
	if (z >= 0)
	{
		SetupDrawNoResize(z);
		newObject = m_doc->Items->at(z);
		newObject->ContourLine = newObject->PoLine.copy();
		m_doc->setRedrawBounding(newObject);
	}
	return newObject;
}

bool CreateMode::doOneClick(FPoint& startPoint, FPoint& endPoint)
{
	bool doCreate = false;
	double xSize, ySize;
	int  originPoint = 0;
	
	if (QApplication::keyboardModifiers() & Qt::ControlModifier)
		return true;

	PrefsContext* sizes = PrefsManager::instance().prefsFile->getContext("ObjectSize");
	bool doRemember     = sizes->getBool("Remember", true);

	int lmode = (createObjectMode == modeDrawLine) ? 1 : 0;
	if (lmode == 0)
	{
		xSize = sizes->getDouble("defWidth", 100.0);
		ySize = sizes->getDouble("defHeight", 100.0);
		originPoint = sizes->getInt("Origin", 0);
	}
	else
	{
		xSize = sizes->getDouble("defLength", 100.0);
		ySize = sizes->getDouble("defAngle", 0.0);
		originPoint = sizes->getInt("OriginL", 0);
	}

	if (lmode == 0)
	{
		if (doRemember)
		{
			sizes->set("defWidth", xSize);
			sizes->set("defHeight", ySize);
			sizes->set("Origin", originPoint);
		}
		endPoint.setXY(startPoint.x() + xSize, startPoint.y() + ySize);
		switch (originPoint)
		{
			case 0:
				break;
			case 1:
				startPoint.setX(startPoint.x() - xSize);
				endPoint.setX(endPoint.x() - xSize);
				break;
			case 2:
				startPoint.setXY(startPoint.x() - xSize / 2.0, startPoint.y() - ySize / 2.0);
				endPoint.setXY(endPoint.x() - xSize / 2.0, endPoint.y() - ySize / 2.0);
				break;
			case 3:
				startPoint.setY(startPoint.y() - ySize);
				endPoint.setY(endPoint.y() - ySize);
				break;
			case 4:
				startPoint.setXY(startPoint.x() - xSize, startPoint.y() - ySize);
				endPoint.setXY(endPoint.x() - xSize, endPoint.y() - ySize);
				break;
		}
	}
	else
	{
		FPoint oldStart = startPoint;
		if (doRemember)
		{
			sizes->set("defLength", xSize);
			sizes->set("defAngle", ySize);
			sizes->set("OriginL", originPoint);
		}
		double angle = -ySize * M_PI / 180.0;
		switch (originPoint)
		{
			case 0:
				endPoint = FPoint(startPoint.x() + xSize * cos(angle), startPoint.y() + xSize * sin(angle));
				break;
			case 1:
				startPoint = FPoint(oldStart.x() - xSize * cos(angle), oldStart.y() - xSize * sin(angle));
				endPoint   = oldStart;
				break;
			case 2:
				startPoint = FPoint(oldStart.x() - xSize / 2.0 * cos(angle), oldStart.y() - xSize / 2.0 * sin(angle));
				endPoint   = FPoint(oldStart.x() + xSize / 2.0 * cos(angle), oldStart.y() + xSize / 2.0 * sin(angle));
				break;
		}
	}
	sizes->set("Remember", doRemember);
	doCreate = true;

	//delete dia;
	return doCreate;
}

FRect CreateMode::adjustedRect(FPoint &firstPoint, FPoint &secondPoint)
{
	// Lock to grid.
	FPoint first = m_doc->ApplyGridF(firstPoint);
	FPoint second = m_doc->ApplyGridF(secondPoint);

	// Lock to guides and items.
	double firstX = first.x();
	double firstY = first.y();
	m_doc->ApplyGuides(&firstX, &firstY);
	m_doc->ApplyGuides(&firstX, &firstY,true);

	double secondX = second.x();
	double secondY = second.y();
	m_doc->ApplyGuides(&secondX, &secondY);
	m_doc->ApplyGuides(&secondX, &secondY,true);

	// Return normalized rectangle.
	FRect rect(firstX, firstY, secondX - firstX, secondY - firstY);

	return rect.normalize();
}

// void CreateMode::setResizeCursor(int how)
// {
// 	switch (how)
// 	{
// 		case 1:
// 		case 2:
// 			qApp->changeOverrideCursor(QCursor(Qt::SizeFDiagCursor));
// 			break;
// 		case 3:
// 		case 4:
// 			qApp->changeOverrideCursor(QCursor(Qt::SizeBDiagCursor));
// 			break;
// 		case 5:
// 		case 8:
// 			qApp->changeOverrideCursor(QCursor(Qt::SizeVerCursor));
// 			break;
// 		case 6:
// 		case 7:
// 			qApp->changeOverrideCursor(QCursor(Qt::SizeHorCursor));
// 			break;
// 		default:
// 			qApp->changeOverrideCursor(QCursor(Qt::SizeAllCursor));
// 			break;
// 	}
// }

