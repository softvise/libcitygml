/* -*-c++-*- libcitygml - Copyright (c) 2010 Joachim Pouderoux, BRGM
*
* Contributors:
*  - Manuel Garnier, BRGM - better normal computation
*
* This file is part of libcitygml library
* http://code.google.com/p/libcitygml
*
* libcitygml is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 2.1 of the License, or
* (at your option) any later version.
*
* libcitygml is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*/

#ifdef WIN32
#define WINDOWS_LEAN_AND_MEAN
#define NOMINMAX
#endif

#include <citygml/tesselator.h>
#ifndef WIN32
#	include <stdint.h>
#endif

#include <citygml/citygml.h>
#include <citygml/citygmllogger.h>

#include <iostream>
#include <assert.h>
#include <algorithm>

Tesselator::Tesselator(std::shared_ptr<citygml::CityGMLLogger> logger, GLenum winding_rule): TesselatorBase(logger), _windingRule(winding_rule)
{
    _tobj = gluNewTess();

    gluTessCallback( _tobj, GLU_TESS_VERTEX_DATA, (GLU_TESS_CALLBACK)&vertexDataCallback );
    gluTessCallback( _tobj, GLU_TESS_BEGIN_DATA, (GLU_TESS_CALLBACK)&beginCallback );
    gluTessCallback( _tobj, GLU_TESS_END_DATA, (GLU_TESS_CALLBACK)&endCallback );
    gluTessCallback( _tobj, GLU_TESS_COMBINE_DATA, (GLU_TESS_CALLBACK)&combineCallback );
    gluTessCallback( _tobj, GLU_TESS_ERROR_DATA, (GLU_TESS_CALLBACK)&errorCallback );
}

void Tesselator::init( const TVec3d& normal)
{
    TesselatorBase::init(normal);
    gluTessBeginPolygon( _tobj, this );

    gluTessProperty( _tobj, GLU_TESS_WINDING_RULE, _windingRule);
    gluTessNormal( _tobj, normal.x, normal.y, normal.z );
}

Tesselator::~Tesselator()
{
    gluDeleteTess( _tobj );
}

void Tesselator::compute()
{
    gluTessEndPolygon( _tobj );
}

void Tesselator::addContour(const std::vector<TVec3d>& pts, std::vector<std::vector<TVec2f> > textureCoordinatesLists )
{
    unsigned int pos = _vertices.size();
    TesselatorBase::addContour(pts, textureCoordinatesLists);

    gluTessBeginContour( _tobj );

    unsigned int len = pts.size();
    for ( unsigned int i = 0; i < len; i++ )
    {
        gluTessVertex( _tobj, &(_vertices[pos + i][0]), &_indices[pos + i] );
    }

    gluTessEndContour( _tobj );
}

void CALLBACK Tesselator::beginCallback( GLenum which, void* userData )
{
    Tesselator* tess = static_cast<Tesselator*>(userData);
    tess->_curMode = which;
}

void CALLBACK Tesselator::vertexDataCallback( GLvoid *data, void* userData )
{
    Tesselator *tess = static_cast<Tesselator*>(userData);
    unsigned int index = *reinterpret_cast<unsigned int*>(data);

    assert(index < tess->_vertices.size());

    tess->_curIndices.push_back( index );
}

void CALLBACK Tesselator::combineCallback( GLdouble coords[3], void* vertex_data[4], GLfloat weight[4], void** outData, void* userData )
{
    Tesselator *tess = static_cast<Tesselator*>(userData);
    tess->_indices.push_back(tess->_indices.size());
    tess->_vertices.push_back( TVec3d( coords[0], coords[1], coords[2] ) );

    if (!tess->_texCoordsLists.empty()) {

        for (std::vector<TVec2f>& texcords : tess->_texCoordsLists) {

            if (texcords.empty()) {
                continue;
            }

            TVec2f newTexCoord(0,0);

            for (int i = 0; i < 4; i++) {
                if (vertex_data[i] != nullptr) {
                    unsigned int vertexIndex = *reinterpret_cast<unsigned int*>(vertex_data[i]);
                    newTexCoord = newTexCoord + weight[i] * texcords.at(vertexIndex);
                }
            }

            texcords.push_back(newTexCoord);

            assert(texcords.size() == tess->_vertices.size());

        }
    }

    *outData = &tess->_indices.back();
}

void CALLBACK Tesselator::endCallback( void* userData )
{
    Tesselator *tess = static_cast<Tesselator*>(userData);

    unsigned int len = tess->_curIndices.size();

    switch ( tess->_curMode )
    {
    case GL_TRIANGLES:
        for ( unsigned int i = 0; i < len; i++ ) tess->_outIndices.push_back( tess->_curIndices[i] );
        break;
    case GL_TRIANGLE_FAN:
    case GL_TRIANGLE_STRIP:
        {
            unsigned int first = tess->_curIndices[0];
            unsigned int prev = tess->_curIndices[1];

            assert(first < tess->_vertices.size());
            assert(prev < tess->_vertices.size());

            for ( unsigned int i = 2; i < len; i++ )
            {
                if ( tess->_curMode == GL_TRIANGLE_FAN || i%2 == 0 ) tess->_outIndices.push_back( first );
                tess->_outIndices.push_back( prev );
                if ( tess->_curMode == GL_TRIANGLE_STRIP )
                {
                    if ( i%2 == 1) tess->_outIndices.push_back( first );
                    first = prev;
                }
                prev = tess->_curIndices[i];
                tess->_outIndices.push_back( prev );
            }
        }
        break;
    default:
        CITYGML_LOG_WARN(tess->_logger, "Tesselator: non-supported GLU tesselator primitive " << tess->_curMode);
    }

    tess->_curIndices.clear();
}

void CALLBACK Tesselator::errorCallback( GLenum errorCode, void* userData )
{
    Tesselator *tess = static_cast<Tesselator*>(userData);
    CITYGML_LOG_ERROR(tess->_logger, "Tesselator error: " << gluErrorString( errorCode ));
}
