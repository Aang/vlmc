/*****************************************************************************
 * ImportMediaListController.cpp
 *****************************************************************************
 * Copyright (C) 2008-2010 VideoLAN
 *
 * Authors: Geoffroy Lacarriere <geoffroylaca@gmail.com>
 *          Thomas Boquet <thomas.boquet@gmail.com>
 *          Clement CHAVANCE <chavance.c@gmail.com>
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

#include "ImportMediaListController.h"
#include <QDebug>

ImportMediaListController::ImportMediaListController( StackViewController* nav ) :
        ListViewController( nav ), m_nav( nav ), m_clipDeleted( 0 )
{
    m_mediaCellList = new QHash<QUuid, ImportMediaCellView*>();
}

ImportMediaListController::~ImportMediaListController()
{
    delete m_mediaCellList;
}

void
ImportMediaListController::addMedia( Media* media )
{
    ImportMediaCellView* cell = new ImportMediaCellView( media->uuid() );
    connect( cell, SIGNAL( cellSelected( const QUuid& ) ),
             this, SIGNAL( mediaSelected( const QUuid& ) ) );
    connect( cell, SIGNAL( cellDeleted( const QUuid& ) ),
             this, SIGNAL( mediaDeleted( const QUuid& ) ) );
    connect( cell, SIGNAL( arrowClicked( const QUuid& ) ),
             this, SIGNAL( showClipListAsked( const QUuid& ) ) );

    cell->setTitle( media->fileName() );
    cell->setThumbnail( media->snapshot() );
    addCell( cell );

    m_mediaCellList->insert( media->uuid(), cell );
    if ( media->baseClip() == NULL )
        connect( media, SIGNAL( metaDataComputed( const Media* ) ),
                 cell, SLOT( enableCell() ) );
}

void
ImportMediaListController::metaDataComputed( const Media* media )
{
    m_mediaCellList->value( media->uuid() )->setThumbnail( media->snapshot() );
}

ImportMediaCellView*
ImportMediaListController::cell( QUuid uuid ) const
{
    if (m_mediaCellList->contains( uuid ) )
        return m_mediaCellList->value( uuid );
    return NULL;
}

bool
ImportMediaListController::contains( QUuid uuid )
{
    return m_mediaCellList->contains( uuid );
}

void
ImportMediaListController::removeMedia( const QUuid& uuid )
{
    const QUuid saveUuid = uuid;
    removeCell( m_mediaCellList->value( uuid ) );
    m_mediaCellList->remove( saveUuid );
}

void
ImportMediaListController::addClip( Clip* clip )
{
    ImportMediaCellView* cell = new ImportMediaCellView( clip->uuid() );
    cell->containsClip();
    connect( cell, SIGNAL( cellSelected( const QUuid& ) ),
             this, SIGNAL( clipSelected( const QUuid& ) ) );
    connect( cell, SIGNAL( cellDeleted( const QUuid& ) ),
             this, SLOT( clipDeletion( const QUuid& ) ) );

    QString size;

    size.setNum( m_mediaCellList->size() + 1 );

    cell->setTitle( clip->getParent()->fileName() + "_" + size );
    cell->setThumbnail( clip->getParent()->snapshot() );
    cell->setLength( clip->lengthSecond(), false  );
    cell->setEnabled( true );
    addCell( cell );

    m_mediaCellList->insert( clip->uuid(), cell );
}

void
ImportMediaListController::removeClip( const QUuid& uuid )
{
    removeCell( m_mediaCellList->value( uuid ) );
    m_mediaCellList->remove( uuid );
}

void
ImportMediaListController::cleanAll()
{
    QUuid uuid;
    MediaCellView* cell;
    foreach( uuid, m_mediaCellList->keys() )
    {
        cell = m_mediaCellList->value( uuid );
        removeCell( cell );
    }
    m_mediaCellList->clear();
}

void
ImportMediaListController::addClipsFromMedia( Media* media )
{
    QUuid uuid;
    foreach( uuid, media->clips()->keys() )
        addClip( media->clips()->value( uuid ) );
}

void
ImportMediaListController::clipDeletion( const QUuid& uuid )
{
    m_clipDeleted += 1;
    emit clipDeleted( uuid );
}

void
ImportMediaListController::clipAdded( Clip* clip )
{
    if ( clip->getParent() == 0 )
        return ;
    const QUuid& uuid = clip->getParent()->uuid();
    if ( m_mediaCellList->contains( uuid ) )
        m_mediaCellList->value( uuid )->incrementClipCount();
}

const QHash<QUuid, ImportMediaCellView*>*
ImportMediaListController::mediaCellList() const
{
    return m_mediaCellList;
}
