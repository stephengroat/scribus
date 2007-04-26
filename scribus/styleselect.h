/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Scribus 1.3.2
for which a new license (GPL+exception) is in place.
*/
#ifndef STYLESELECT_H
#define STYLESELECT_H

class QGroupBox;
class QToolButton;
class QLayout;
class MSpinBox;
class QLabel;
class QPopupMenu;

#include "scribusapi.h"

class SCRIBUS_API StrikeValues : public QGroupBox
{
    Q_OBJECT

public:
	StrikeValues( QWidget* parent );
	~StrikeValues() {};
	MSpinBox* LPos;
	MSpinBox* LWidth;
	QLabel* LPosTxt;
	QLabel* LWidthTxt;

public slots:
	void languageChange();
	
protected:
	QGridLayout* group1Layout;
};

class SCRIBUS_API UnderlineValues : public QGroupBox
{
    Q_OBJECT

public:
	UnderlineValues( QWidget* parent );
	~UnderlineValues() {};
	MSpinBox* LPos;
	MSpinBox* LWidth;
	QLabel* LPosTxt;
	QLabel* LWidthTxt;

public slots:
	void languageChange();
	
protected:
	QGridLayout* group1Layout;
};

class SCRIBUS_API OutlineValues : public QGroupBox
{
    Q_OBJECT

public:
	OutlineValues( QWidget* parent );
	~OutlineValues() {};
	MSpinBox* LWidth;
	QLabel* LWidthTxt;

public slots:
	void languageChange();
	
protected:
	QGridLayout* group1Layout;
};

class SCRIBUS_API ShadowValues : public QGroupBox
{
    Q_OBJECT

public:
	ShadowValues( QWidget* parent );
	~ShadowValues() {};
	MSpinBox* Xoffset;
	MSpinBox* Yoffset;
	QLabel* XoffsetTxt;
	QLabel* YoffsetTxt;

public slots:
	void languageChange();
	
protected:
	QGridLayout* group1Layout;
};

class SCRIBUS_API StyleSelect : public QWidget
{

	Q_OBJECT

public:
	StyleSelect(QWidget* parent);
	~StyleSelect() {};
	void setStyle(int s);
	int getStyle();
	ShadowValues* ShadowVal;
	OutlineValues* OutlineVal;
	UnderlineValues* UnderlineVal;
	StrikeValues* StrikeVal;

public slots:
	void languageChange();

private slots:
	void setTypeStyle();

signals:
	void State(int);

protected:
	QHBoxLayout* ssLayout;
	QToolButton* underlineButton;
	QToolButton* underlineWordButton;
	QToolButton* subscriptButton;
	QToolButton* superscriptButton;
	QToolButton* smallcapsButton;
	QToolButton* allcapsButton;
	QToolButton* strikeoutButton;
	QToolButton* outlineButton;
	QToolButton* shadowButton;
	QPopupMenu* ShadowPop;
	QPopupMenu* OutlinePop;
	QPopupMenu* UnderlinePop;
	QPopupMenu* StrikePop;
};

#endif

