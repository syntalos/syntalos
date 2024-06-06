/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2012, 2013 Jure Varlec <jure.varlec@ad-vega.si>
                             Andrej Lajovic <andrej.lajovic@ad-vega.si>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef QARVFEATURETREE_H
#define QARVFEATURETREE_H

#include "qarvcamera.h"

#include <arv.h>
#include <gio/gio.h>

//! A class that stores the hirearchy of camera features.
/*! String identifiers are used to get feature nodes from Aravis. At first it
 * seems that a QAbstractItemModel can be implemented by only using Aravis
 * functions to walk the feature hirearchy, but it turns out there is no way
 * to find a feature's parent that way. Also, string identifiers returned by
 * Aravis are not persistent and need to be copied. Therefore, a tree to store
 * feature identifiers is used by the model. It is assumed that the hirearchy
 * is static.
 */
class QArvCamera::QArvFeatureTree {
public:
    QArvFeatureTree(QArvFeatureTree* parent = NULL, const char* feature = NULL);
    ~QArvFeatureTree();
    QArvFeatureTree* parent();
    QList<QArvFeatureTree*> children();
    const char* feature();
    int row();
    static QArvFeatureTree* createFeaturetree(ArvGc* cam);
    static void recursiveSerialization(QTextStream& out, QArvCamera* camera,
                                       QArvFeatureTree* tree);
    static void freeFeaturetree(QArvFeatureTree* tree);

private:
    void addChild(QArvFeatureTree* child);
    void removeChild(QArvFeatureTree* child);
    static void recursiveMerge(ArvGc* cam,
                               QArvFeatureTree* tree,
                               ArvGcNode* node);
    QArvFeatureTree* parent_;
    QList<QArvFeatureTree*> children_;
    const char* feature_;
};

#endif
