/*****************************************************************************
 * GraphicsAudioItem.cpp: Represent an audio region graphically in the
 * timeline
 *****************************************************************************
 * Copyright (C) 2008-2010 VideoLAN
 *
 * Authors: Ludovic Fauvet <etix@l0cal.com>
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

#include <QPainter>
#include <QLinearGradient>
#include <QDebug>
#include <QTime>
#include "GraphicsAudioItem.h"
#include "TracksView.h"
#include "Timeline.h"

GraphicsAudioItem::GraphicsAudioItem( Clip* clip ) : m_clip( clip )
{
    setFlags( QGraphicsItem::ItemIsSelectable );

    QTime length = QTime().addMSecs( clip->getParent()->lengthMS() );
    QString tooltip( tr( "<p style='white-space:pre'><b>Name:</b> %1"
                     "<br><b>Length:</b> %2" )
                     .arg( clip->getParent()->fileName() )
                     .arg( length.toString("hh:mm:ss.zzz") ) );
    setToolTip( tooltip );
    setAcceptHoverEvents( true );

    // Adjust the width
    setWidth( clip->length() );
    // Automatically adjust future changes
    connect( clip, SIGNAL( lengthUpdated() ), this, SLOT( adjustLength() ) );
}

GraphicsAudioItem::~GraphicsAudioItem()
{
}

MainWorkflow::TrackType GraphicsAudioItem::mediaType() const
{
    return MainWorkflow::AudioTrack;
}

void GraphicsAudioItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* )
{
    painter->save();
    paintRect( painter, option );
    painter->restore();

    painter->save();
    paintTitle( painter, option );
    painter->restore();
}

Clip* GraphicsAudioItem::clip() const
{
    return m_clip;
}

void GraphicsAudioItem::paintRect( QPainter* painter, const QStyleOptionGraphicsItem* option )
{
    QRectF drawRect;
    bool drawRound;

    // Disable the matrix transformations
    painter->setWorldMatrixEnabled( false );

    painter->setRenderHint( QPainter::Antialiasing );

    // Get the transformations required to map the text on the viewport
    QTransform viewPortTransform = Timeline::getInstance()->tracksView()->viewportTransform();

    // Determine if a drawing optimization can be used
    if ( option->exposedRect.left() > ROUNDED_RECT_RADIUS &&
         option->exposedRect.right() < boundingRect().right() - ROUNDED_RECT_RADIUS )
    {
        // Optimized: paint only the exposed (horizontal) area
        drawRect = QRectF( option->exposedRect.left(),
                           boundingRect().top(),
                           option->exposedRect.right(),
                           boundingRect().bottom() );
        drawRound = false;
    }
    else
    {
        // Unoptimized: the item must be fully repaint
        drawRect = boundingRect();
        drawRound = true;
    }

    // Do the transformation
    QRectF mapped = deviceTransform( viewPortTransform ).mapRect( drawRect );

    QLinearGradient gradient( mapped.topLeft(), mapped.bottomLeft() );

    gradient.setColorAt( 0, QColor::fromRgb( 88, 88, 88 ) );
    gradient.setColorAt( 0.4, QColor::fromRgb( 82, 82, 82 ) );
    gradient.setColorAt( 0.4, QColor::fromRgb( 60, 60, 60 ) );
    gradient.setColorAt( 1, QColor::fromRgb( 55, 55, 55 ) );

    painter->setPen( Qt::NoPen );
    painter->setBrush( QBrush( gradient ) );

    if ( drawRound )
        painter->drawRoundedRect( mapped, ROUNDED_RECT_RADIUS, ROUNDED_RECT_RADIUS );
    else
        painter->drawRect( mapped );

    if ( itemColor().isValid() )
    {
        QRectF mediaColorRect = mapped.adjusted( 3, 2, -3, -2 );
        painter->setPen( QPen( itemColor(), 2 ) );
        painter->drawLine( mediaColorRect.topLeft(), mediaColorRect.topRight() );
    }

    if ( isSelected() )
    {
        setZValue( Z_SELECTED );
        painter->setPen( Qt::yellow );
        painter->setBrush( Qt::NoBrush );
        mapped.adjust( 0, 0, 0, -1 );
        if ( drawRound )
            painter->drawRoundedRect( mapped, ROUNDED_RECT_RADIUS, ROUNDED_RECT_RADIUS );
        else
            painter->drawRect( mapped );
    }
    else
        setZValue( Z_NOT_SELECTED );
}

void GraphicsAudioItem::paintTitle( QPainter* painter, const QStyleOptionGraphicsItem* option )
{
    Q_UNUSED( option );

    // Disable the matrix transformations
    painter->setWorldMatrixEnabled( false );

    // Setup the font
    QFont f = painter->font();
    f.setPointSize( 8 );
    painter->setFont( f );

    // Initiate the font metrics calculation
    QFontMetrics fm( painter->font() );
    QString text = m_clip->getParent()->fileName();

    // Get the transformations required to map the text on the viewport
    QTransform viewPortTransform = Timeline::getInstance()->tracksView()->viewportTransform();
    // Do the transformation
    QRectF mapped = deviceTransform( viewPortTransform ).mapRect( boundingRect() );
    // Create an inner rect
    mapped.adjust( 2, 2, -2, -2 );

    painter->setPen( Qt::white );
    painter->drawText( mapped, Qt::AlignVCenter, fm.elidedText( text, Qt::ElideRight, mapped.width() ) );
}

void GraphicsAudioItem::hoverEnterEvent( QGraphicsSceneHoverEvent* event )
{
    TracksView* tv = Timeline::getInstance()->tracksView();
    if ( tv )
    {
        switch ( tv->tool() )
        {
            case TOOL_DEFAULT:
            setCursor( Qt::OpenHandCursor );
            break;

            case TOOL_CUT:
            setCursor( QCursor( QPixmap( ":/images/editcut" ) ) );
            break;
        }
    }

    AbstractGraphicsMediaItem::hoverEnterEvent( event );
}

void GraphicsAudioItem::hoverLeaveEvent( QGraphicsSceneHoverEvent* event )
{
    AbstractGraphicsMediaItem::hoverLeaveEvent( event );
}

void GraphicsAudioItem::hoverMoveEvent( QGraphicsSceneHoverEvent* event )
{
    if ( !tracksView() ) return;

    if ( tracksView()->tool() == TOOL_DEFAULT )
    {
        if ( resizeZone( event->pos() ) )
            setCursor( Qt::SizeHorCursor );
        else
            setCursor( Qt::OpenHandCursor );
    }
}

void GraphicsAudioItem::mousePressEvent( QGraphicsSceneMouseEvent* event )
{
    TracksView* tv = Timeline::getInstance()->tracksView();
    if ( tv->tool() == TOOL_DEFAULT )
        setCursor( Qt::ClosedHandCursor );
    else if ( tv->tool() == TOOL_CUT )
        emit split( this, qRound64( event->pos().x() ) );
}

void GraphicsAudioItem::mouseReleaseEvent( QGraphicsSceneMouseEvent*  event )
{
    Q_UNUSED( event );
    TracksView* tv = Timeline::getInstance()->tracksView();
    if ( tv->tool() == TOOL_DEFAULT )
        setCursor( Qt::OpenHandCursor );
}
