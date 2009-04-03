/*****************************************************************************
 * PreviewWidget.cpp: Preview widget
 *****************************************************************************
 * Copyright (C) 2008-2009 the VLMC team
 *
 * Authors: Geoffroy Lacarrière <geoffroylaca@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef PREVIEWWIDGET_H
#define PREVIEWWIDGET_H

#include <QtGui/QDialog>
#include <QDragEnterEvent>

#include "InputMedia.h"

namespace Ui {
    class PreviewWidget;
}

class PreviewWidget : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY( PreviewWidget )
public:
    explicit PreviewWidget( QWidget *parent = 0 );
    virtual ~PreviewWidget();

protected:
    virtual void    changeEvent( QEvent *e );
    virtual void    dragEnterEvent( QDragEnterEvent* event );
    virtual void    dropEvent( QDropEvent* event );

private slots:
    void            videoTimeChanged();

private:
    Ui::PreviewWidget*      m_ui;
    InputMedia*             m_currentMedia;
    LibVLCpp::Instance*     m_currentInstance;
};

#endif // PREVIEWWIDGET_H
