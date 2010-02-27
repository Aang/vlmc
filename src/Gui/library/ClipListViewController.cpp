/*****************************************************************************
 * ClipListViewController.cpp: controller for the library clip list
 *****************************************************************************
 * Copyright (C) 2008-2010 VideoLAN
 *
 * Authors: Clement CHAVANCE <chavance.c@gmail.com>
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

#include <QtDebug>
#include <QWidget>
#include "ClipListViewController.h"
#include "MediaCellView.h"
#include "Library.h"

ClipListViewController::ClipListViewController( StackViewController* nav, const QUuid& mediaId )
    : ListViewController( nav ),
    m_nav( nav ),
    m_mediaId( mediaId ),
    m_currentUuid(),
    m_deletion( 0 )
{
    connect( this, SIGNAL( clipDeleted( const QUuid&, const QUuid& ) ),
             Library::getInstance(), SLOT( removeClip( const QUuid&, const QUuid& ) ) );
}

ClipListViewController::~ClipListViewController()
{
    foreach( QWidget* cell, m_cells.values() )
        removeCell( cell );
}

void    ClipListViewController::addClipsFromMedia( Media* media )
{
    foreach( Clip* clip, *media->clips() )
        addClip( clip );
}

int     ClipListViewController::getNbDeletion() const
{
    return m_deletion;
}

void    ClipListViewController::resetNbDeletion()
{
    m_deletion = 0;
}

void    ClipListViewController::addClip( Clip* clip )
{
    MediaCellView* cell = new MediaCellView( clip->uuid() );
    cell->containsClip();
    connect( cell, SIGNAL( cellSelected( QUuid ) ), this, SLOT( cellSelection( const QUuid& ) ) );
    connect( cell, SIGNAL( cellDeleted( const QUuid& ) ), this, SLOT( clipDeletion( const QUuid& ) ) );

    cell->setThumbnail( clip->getParent()->snapshot() );
    QString number;
    number.setNum( m_cells.size() + 1 );
    cell->setTitle( clip->getParent()->fileName() + number );
    cell->setLength( clip->lengthSecond(), false );
    addCell( cell );
    m_cells.insert( clip->uuid(), cell );
    cell->enableCell();
}

void    ClipListViewController::clean()
{
    foreach ( QWidget* widg, m_cells )
    {
        removeCell( widg );
        delete widg;
    }
    m_cells.clear();
}

void    ClipListViewController::cellSelection( const QUuid& uuid )
{
    if ( m_currentUuid == uuid )
        return;

    if ( m_cells.contains( uuid ) )
    {
        if ( !m_currentUuid.isNull() && m_cells.contains( m_currentUuid ) )
            m_cells.value( m_currentUuid )->setPalette( m_cells.value( uuid )->palette() );
        QPalette p = m_cells.value( uuid )->palette();
        p.setColor( QPalette::Window, QColor( Qt::darkBlue ) );
        m_cells.value( uuid )->setPalette( p );
        m_currentUuid = uuid;
        emit clipSelected( uuid );
    }
}

void    ClipListViewController::clipDeletion( const QUuid& uuid )
{
    if ( m_cells.contains( uuid ) )
    {
        QUuid tmp = uuid;
        ++m_deletion;
        QWidget* cell = m_cells.value( uuid );
        removeCell( cell );
        m_currentUuid = QUuid();
        m_cells.remove( tmp );
        emit clipDeleted( m_mediaId, tmp );
    }
}
