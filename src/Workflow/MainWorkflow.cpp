/*****************************************************************************
 * MainWorkflow.cpp : Will query all of the track workflows to render the final
 *                    image
 *****************************************************************************
 * Copyright (C) 2008-2010 VideoLAN
 *
 * Authors: Hugo Beauzee-Luyssen <hugo@vlmc.org>
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

#include "vlmc.h"
#include "Clip.h"
#include "EffectsEngine.h"
#include "Library.h"
#include "LightVideoFrame.h"
#include "MainWorkflow.h"
#include "TrackWorkflow.h"
#include "TrackHandler.h"
#include "SettingsManager.h"

#include <QDomElement>

LightVideoFrame     *MainWorkflow::blackOutput = NULL;

MainWorkflow::MainWorkflow( int trackCount ) :
        m_lengthFrame( 0 ),
        m_renderStarted( false ),
        m_width( 0 ),
        m_height( 0 )
{
    m_currentFrameLock = new QReadWriteLock;
    m_renderStartedMutex = new QMutex;

    m_effectEngine = new EffectsEngine;
    m_effectEngine->disable();

    m_tracks = new TrackHandler*[MainWorkflow::NbTrackType];
    m_currentFrame = new qint64[MainWorkflow::NbTrackType];
    for ( unsigned int i = 0; i < MainWorkflow::NbTrackType; ++i )
    {
        MainWorkflow::TrackType trackType =
                (i == 0 ? MainWorkflow::VideoTrack : MainWorkflow::AudioTrack );
        m_tracks[i] = new TrackHandler( trackCount, trackType, m_effectEngine );
        connect( m_tracks[i], SIGNAL( tracksEndReached() ),
                 this, SLOT( tracksEndReached() ) );
        m_currentFrame[i] = 0;
    }
    m_outputBuffers = new OutputBuffers;
}

MainWorkflow::~MainWorkflow()
{
    //FIXME: this is probably useless, since already done by the renderer
    stop();

    delete m_effectEngine;
    delete m_renderStartedMutex;
    delete m_currentFrameLock;
    delete m_currentFrame;
    for ( unsigned int i = 0; i < MainWorkflow::NbTrackType; ++i )
        delete m_tracks[i];
    delete[] m_tracks;
}

EffectsEngine*
MainWorkflow::getEffectsEngine()
{
    return m_effectEngine;
}

void
MainWorkflow::addClip( Clip *clip, unsigned int trackId,
                                        qint64 start, MainWorkflow::TrackType trackType )
{
    m_tracks[trackType]->addClip( clip, trackId, start );
    computeLength();
    //Inform the GUI
    emit clipAdded( clip, trackId, start, trackType );
}

void
MainWorkflow::computeLength()
{
    qint64      maxLength = 0;

    for ( unsigned int i = 0; i < MainWorkflow::NbTrackType; ++i )
    {
        if ( m_tracks[i]->getLength() > maxLength )
            maxLength = m_tracks[i]->getLength();
    }
    if ( m_lengthFrame != maxLength )
    {
        m_lengthFrame = maxLength;
        emit lengthChanged( m_lengthFrame );
    }

}

void
MainWorkflow::startRender( quint32 width, quint32 height )
{
    m_renderStarted = true;
    m_width = width;
    m_height = height;
    if ( blackOutput != NULL )
        delete blackOutput;
    blackOutput = new LightVideoFrame( m_width, m_height );
    // FIX ME vvvvvv , It doesn't update meta info (nbpixels, nboctets, etc.
    memset( (*blackOutput)->frame.octets, 0, (*blackOutput)->nboctets );
    for ( unsigned int i = 0; i < MainWorkflow::NbTrackType; ++i )
        m_tracks[i]->startRender();
    computeLength();
}

MainWorkflow::OutputBuffers*
MainWorkflow::getOutput( TrackType trackType, bool paused )
{
    QMutexLocker        lock( m_renderStartedMutex );

    if ( m_renderStarted == true )
    {
        QReadLocker         lock2( m_currentFrameLock );

        m_tracks[trackType]->getOutput( m_currentFrame[VideoTrack],
                                        m_currentFrame[trackType], paused );
        if ( trackType == MainWorkflow::VideoTrack )
        {
            m_effectEngine->render();
            const LightVideoFrame &tmp = m_effectEngine->getVideoOutput( 1 );
            if ( tmp->nboctets == 0 )
                m_outputBuffers->video = blackOutput;
            else
                m_outputBuffers->video = &tmp;
        }
        else
        {
            m_outputBuffers->audio =
                    m_tracks[MainWorkflow::AudioTrack]->getTmpAudioBuffer();
        }
    }
    return m_outputBuffers;
}

void
MainWorkflow::nextFrame( MainWorkflow::TrackType trackType )
{
    QWriteLocker    lock( m_currentFrameLock );

    ++m_currentFrame[trackType];
    if ( trackType == MainWorkflow::VideoTrack )
        emit frameChanged( m_currentFrame[MainWorkflow::VideoTrack], Renderer );
}

void
MainWorkflow::previousFrame( MainWorkflow::TrackType trackType )
{
    QWriteLocker    lock( m_currentFrameLock );

    --m_currentFrame[trackType];
    if ( trackType == MainWorkflow::VideoTrack )
        emit frameChanged( m_currentFrame[MainWorkflow::VideoTrack], Renderer );
}

qint64
MainWorkflow::getLengthFrame() const
{
    return m_lengthFrame;
}

qint64
MainWorkflow::getClipPosition( const QUuid& uuid, unsigned int trackId,
                               MainWorkflow::TrackType trackType ) const
{
    return m_tracks[trackType]->getClipPosition( uuid, trackId );
}

void
MainWorkflow::stop()
{
    QMutexLocker    lock( m_renderStartedMutex );
    QWriteLocker    lock2( m_currentFrameLock );

    m_renderStarted = false;
    for (unsigned int i = 0; i < MainWorkflow::NbTrackType; ++i)
    {
        m_tracks[i]->stop();
        m_currentFrame[i] = 0;
    }
    emit frameChanged( 0, Renderer );
}

void
MainWorkflow::moveClip( const QUuid &clipUuid, unsigned int oldTrack,
                        unsigned int newTrack, qint64 startingFrame,
                        MainWorkflow::TrackType trackType,
                        bool undoRedoCommand /*= false*/ )
{
    m_tracks[trackType]->moveClip( clipUuid, oldTrack, newTrack, startingFrame );
    computeLength();

    if ( undoRedoCommand == true )
    {
        emit clipMoved( clipUuid, newTrack, startingFrame, trackType );
    }
}

Clip*
MainWorkflow::removeClip( const QUuid &uuid, unsigned int trackId,
                          MainWorkflow::TrackType trackType )
{
    Clip *clip = m_tracks[trackType]->removeClip( uuid, trackId );
    if ( clip != NULL )
    {
        computeLength();
        emit clipRemoved( clip, trackId, trackType );
    }
    return clip;
}

void
MainWorkflow::muteTrack( unsigned int trackId, MainWorkflow::TrackType trackType )
{
    m_tracks[trackType]->muteTrack( trackId );
}

void
MainWorkflow::unmuteTrack( unsigned int trackId, MainWorkflow::TrackType trackType )
{
    m_tracks[trackType]->unmuteTrack( trackId );
}

void
MainWorkflow::muteClip( const QUuid& uuid, unsigned int trackId,
                        MainWorkflow::TrackType trackType )
{
    m_tracks[trackType]->muteClip( uuid, trackId );
}

void
MainWorkflow::unmuteClip( const QUuid& uuid, unsigned int trackId,
                          MainWorkflow::TrackType trackType )
{
    m_tracks[trackType]->unmuteClip( uuid, trackId );
}

void toggleBreakPoint()
{
}

void
MainWorkflow::setCurrentFrame( qint64 currentFrame, MainWorkflow::FrameChangedReason reason )
{
    QWriteLocker    lock( m_currentFrameLock );

    toggleBreakPoint();
    if ( m_renderStarted == true )
    {
        //Since any track can be reactivated, we reactivate all of them, and let them
        //disable themself if required.
        for ( unsigned int i = 0; i < MainWorkflow::NbTrackType; ++i)
            m_tracks[i]->activateAll();
    }
    for ( unsigned int i = 0; i < MainWorkflow::NbTrackType; ++i)
        m_currentFrame[i] = currentFrame;
    emit frameChanged( currentFrame, reason );
}

Clip*
MainWorkflow::getClip( const QUuid &uuid, unsigned int trackId,
                       MainWorkflow::TrackType trackType )
{
    return m_tracks[trackType]->getClip( uuid, trackId );
}

/**
 *  \warning    The mainworkflow is expected to be already cleared by the ProjectManager
 */
void
MainWorkflow::loadProject( const QDomElement &project )
{
    if ( project.isNull() == true || project.tagName() != "timeline" )
    {
        qWarning() << "Invalid timeline node (" << project.tagName() << ')';
        return ;
    }

    QDomElement elem = project.firstChild().toElement();

    while ( elem.isNull() == false )
    {
        bool    ok;

        Q_ASSERT( elem.tagName() == "track" );
        unsigned int trackId = elem.attribute( "id" ).toUInt( &ok );
        if ( ok == false )
        {
            qWarning() << "Invalid track number in project file";
            return ;
        }
        QDomElement clip = elem.firstChild().toElement();
        while ( clip.isNull() == false )
        {
            //Iterate over clip fields:
            QDomElement clipProperty = clip.firstChild().toElement();
            QUuid                       parent;
            qint64                      begin;
            qint64                      end;
            qint64                      startPos;
            MainWorkflow::TrackType     trackType = MainWorkflow::VideoTrack;

            while ( clipProperty.isNull() == false )
            {
                QString tagName = clipProperty.tagName();
                bool    ok;

                if ( tagName == "parent" )
                    parent = QUuid( clipProperty.text() );
                else if ( tagName == "begin" )
                {
                    begin = clipProperty.text().toLongLong( &ok );
                    if ( ok == false )
                    {
                        qWarning() << "Invalid clip begin";
                        return ;
                    }
                }
                else if ( tagName == "end" )
                {
                    end = clipProperty.text().toLongLong( &ok );
                    if ( ok == false )
                    {
                        qWarning() << "Invalid clip end";
                        return ;
                    }
                }
                else if ( tagName == "startFrame" )
                {
                    startPos = clipProperty.text().toLongLong( &ok );
                    if ( ok == false )
                    {
                        qWarning() << "Invalid clip starting frame";
                        return ;
                    }
                }
                else if ( tagName == "trackType" )
                {
                    trackType = static_cast<MainWorkflow::TrackType>(
                                                    clipProperty.text().toUInt( &ok ) );
                    if ( ok == false )
                    {
                        qWarning() << "Invalid track type starting frame";
                        return ;
                    }
                }
                else
                    qDebug() << "Unknown field" << clipProperty.tagName();

                clipProperty = clipProperty.nextSibling().toElement();
            }

            if ( Library::getInstance()->media( parent ) != NULL )
            {
                Clip        *c = new Clip( parent, begin, end );
                addClip( c, trackId, startPos, trackType );
            }

            clip = clip.nextSibling().toElement();
        }
        elem = elem.nextSibling().toElement();
    }
}

void
MainWorkflow::saveProject( QDomDocument& doc, QDomElement& rootNode )
{
    QDomElement project = doc.createElement( "timeline" );
    for ( unsigned int i = 0; i < MainWorkflow::NbTrackType; ++i )
    {
        m_tracks[i]->save( doc, project );
    }
    rootNode.appendChild( project );
}

void
MainWorkflow::clear()
{
    for ( unsigned int i = 0; i < MainWorkflow::NbTrackType; ++i )
        m_tracks[i]->clear();
    emit cleared();
}

void
MainWorkflow::tracksEndReached()
{
    for ( unsigned int i = 0; i < MainWorkflow::NbTrackType; ++i )
    {
        if ( m_tracks[i]->endIsReached() == false )
            return ;
    }
    emit mainWorkflowEndReached();
}

int
MainWorkflow::getTrackCount( MainWorkflow::TrackType trackType ) const
{
    return m_tracks[trackType]->getTrackCount();
}

qint64
MainWorkflow::getCurrentFrame() const
{
    QReadLocker     lock( m_currentFrameLock );

    return m_currentFrame[MainWorkflow::VideoTrack];
}

quint32
MainWorkflow::getWidth() const
{
    Q_ASSERT( m_width != 0 );
    return m_width;
}

quint32
MainWorkflow::getHeight() const
{
    Q_ASSERT( m_height != 0 );
    return m_height;
}

void
MainWorkflow::renderOneFrame()
{
    for ( unsigned int i = 0; i < MainWorkflow::NbTrackType; ++i )
        m_tracks[i]->renderOneFrame();
    nextFrame( VideoTrack );
    nextFrame( AudioTrack );
}

void
MainWorkflow::setFullSpeedRender( bool val )
{
    for ( unsigned int i = 0; i < MainWorkflow::NbTrackType; ++i )
        m_tracks[i]->setFullSpeedRender( val );
}

Clip*
MainWorkflow::split( Clip* toSplit, Clip* newClip, quint32 trackId, qint64 newClipPos, qint64 newClipBegin, MainWorkflow::TrackType trackType )
{
    QMutexLocker    lock( m_renderStartedMutex );

    if ( newClip == NULL )
        newClip = new Clip( toSplit, newClipBegin, toSplit->end() );

    toSplit->setEnd( newClipBegin, true );
    addClip( newClip, trackId, newClipPos, trackType );
    return newClip;
}

void
MainWorkflow::resizeClip( Clip* clip, qint64 newBegin, qint64 newEnd, qint64 newPos,
                          quint32 trackId, MainWorkflow::TrackType trackType,
                                      bool undoRedoAction /*= false*/ )
{
    QMutexLocker    lock( m_renderStartedMutex );

    if ( newBegin != clip->begin() )
    {
        moveClip( clip->uuid(), trackId, trackId, newPos, trackType, undoRedoAction );
    }
    clip->setBoundaries( newBegin, newEnd );
}

void
MainWorkflow::unsplit( Clip* origin, Clip* splitted, quint32 trackId,
                       MainWorkflow::TrackType trackType )
{
    QMutexLocker    lock( m_renderStartedMutex );

    removeClip( splitted->uuid(), trackId, trackType );
    origin->setEnd( splitted->end(), true );
}
