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

#include "qarvfeaturetree.h"
#include "qarvtype.h"

QArvCamera::QArvFeatureTree::QArvFeatureTree(
    QArvCamera::QArvFeatureTree* parent, const char* feature) :
    children_() {
    if (feature == NULL) feature_ = strdup("");
    else feature_ = strdup(feature);
    parent_ = parent;
    if (parent_ != NULL) parent_->addChild(this);
}

QArvCamera::QArvFeatureTree::~QArvFeatureTree() {
    if (feature_ != NULL) free((void*)feature_);
    if (parent_ != NULL) parent_->removeChild(this);
    for (auto child = children_.begin(); child != children_.end(); child++)
        delete *child;
}

void QArvCamera::QArvFeatureTree::addChild(QArvCamera::QArvFeatureTree* child) {
    children_ << child;
}

QList< QArvCamera::QArvFeatureTree* > QArvCamera::QArvFeatureTree::children() {
    return children_;
}

const char* QArvCamera::QArvFeatureTree::feature() {
    return feature_;
}

QArvCamera::QArvFeatureTree* QArvCamera::QArvFeatureTree::parent() {
    return parent_;
}

void QArvCamera::QArvFeatureTree::removeChild(
    QArvCamera::QArvFeatureTree* child) {
    children_.removeAll(child);
}

int QArvCamera::QArvFeatureTree::row() {
    if (parent_ == NULL) return 0;
    auto litter = parent_->children();
    return litter.indexOf(this);
}

//! Walk the Aravis feature tree and copy it as an QArvFeatureTree.
/**@{*/
void QArvCamera::QArvFeatureTree::recursiveMerge(
    ArvGc* cam,
    QArvCamera::QArvFeatureTree*
    tree,
    ArvGcNode* node) {
    const GSList* child = arv_gc_category_get_features(ARV_GC_CATEGORY(node));
    for (; child != NULL; child = child->next) {
        ArvGcNode* newnode = arv_gc_get_node(cam, (const char*)(child->data));
        auto newtree =
            new QArvCamera::QArvFeatureTree(tree, (const char*)(child->data));
        if (ARV_IS_GC_CATEGORY(newnode)) recursiveMerge(cam, newtree, newnode);
    }
}

QArvCamera::QArvFeatureTree* QArvCamera::QArvFeatureTree::createFeaturetree(
    ArvGc* cam) {
    QArvCamera::QArvFeatureTree* tree = new QArvCamera::QArvFeatureTree(NULL,
                                                                        "Root");
    ArvGcNode* node = arv_gc_get_node(cam, tree->feature());
    recursiveMerge(cam, tree, node);
    return tree;
}
/**@}*/

void QArvCamera::QArvFeatureTree::freeFeaturetree(
    QArvCamera::QArvFeatureTree* tree) {
    auto children = tree->children();
    for (auto child = children.begin(); child != children.end(); child++)
        freeFeaturetree(*child);
    delete tree;
}

//! Serialize the tree, used by QArvCamera stream operators.
void QArvCamera::QArvFeatureTree::recursiveSerialization(
    QTextStream& out,
    QArvCamera* camera,
    QArvCamera::
        QArvFeatureTree* tree) {
    auto node = arv_gc_get_node(camera->genicam, tree->feature());

    if (tree->children().count() != 0) {
        if (QString("Root") != tree->feature())
            out << "Category: " << tree->feature() << Qt::endl;
        foreach (auto child, tree->children()) {
            recursiveSerialization(out, camera, child);
        }
        return;
    }

    if (ARV_IS_GC_COMMAND(node)) return;

    // Skip read-only features (diagnostics, temperatures, etc.)
    if (ARV_IS_GC_FEATURE_NODE(node)) {
        auto accessMode = arv_gc_feature_node_get_actual_access_mode(ARV_GC_FEATURE_NODE(node));
        if (accessMode == ARV_GC_ACCESS_MODE_RO)
            return;
    }

    out << "\t" << tree->feature() << "\t";
    if (ARV_IS_GC_REGISTER_NODE(node)
        && QString(arv_dom_node_get_node_name(ARV_DOM_NODE(node)))
        == "IntReg") {
        QArvRegister r;
        r.length = arv_gc_register_get_length(ARV_GC_REGISTER(node), NULL);
        r.value = QByteArray(r.length, 0);
        arv_gc_register_get(ARV_GC_REGISTER(node),
                            r.value.data(), r.length, NULL);
        out << "Register\t" << QString::number(r.length) << "\t"
            << QString("0x") + r.value.toHex() << Qt::endl;
    } else if (ARV_IS_GC_ENUMERATION(node)) {
        out << "Enumeration\t"
            << arv_gc_enumeration_get_string_value(ARV_GC_ENUMERATION(node),
                                               NULL)
            << Qt::endl;
    } else if (ARV_IS_GC_STRING(node)) {
        out << "String\t" << arv_gc_string_get_value(ARV_GC_STRING(node), NULL)
            << Qt::endl;
    } else if (ARV_IS_GC_FLOAT(node)) {
      out << "Float\t"
          << QString::number(arv_gc_float_get_value(ARV_GC_FLOAT(node), NULL), 'g', 17)
          << Qt::endl;
    } else if (ARV_IS_GC_BOOLEAN(node)) {
        out << "Boolean\t" << arv_gc_boolean_get_value(ARV_GC_BOOLEAN(node),
                                                       NULL) << Qt::endl;
    } else if (ARV_IS_GC_INTEGER(node)) {
        out << "Integer\t" << arv_gc_integer_get_value(ARV_GC_INTEGER(node),
                                                       NULL) << Qt::endl;
    }
}
