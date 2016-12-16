#include "testsubjectlistmodel.h"

#include <QJsonObject>

TestSubjectListModel::TestSubjectListModel(const QList<TestSubject> &subjects,
                                           QObject *parent)
    : QAbstractListModel(parent)
{
    m_subjects = subjects;
}

TestSubjectListModel::TestSubjectListModel(QObject *parent)
    : QAbstractListModel(parent)
{}

int TestSubjectListModel::rowCount(const QModelIndex &parent) const
{
    // For list models only the root node (an invalid parent) should return the list's size. For all
    // other (valid) parents, rowCount() should return 0 so that it does not become a tree model.
    if (parent.isValid())
        return 0;

    return m_subjects.count();
}

QVariant TestSubjectListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= m_subjects.size())
        return QVariant();

    if (role == Qt::DisplayRole)
        return m_subjects.at(index.row()).id;
    else
        return QVariant();
}

void TestSubjectListModel::addSubject(const TestSubject subject)
{
    beginInsertRows(QModelIndex(), m_subjects.count(), m_subjects.count());
    m_subjects.append(subject);
    endInsertRows();
}

TestSubject TestSubjectListModel::subject(int row) const
{
    if (row >= m_subjects.count())
        return TestSubject();
    return m_subjects.at(row);
}

bool TestSubjectListModel::removeRows(int position, int rows, const QModelIndex &parent)
{
    Q_UNUSED(parent);

    beginRemoveRows(QModelIndex(), position, position+rows-1);

    for (int row = 0; row < rows; ++row) {
        m_subjects.removeAt(position);
    }

    endRemoveRows();
    return true;
}

bool TestSubjectListModel::removeRow(int row, const QModelIndex &parent)
{
    Q_UNUSED(parent);

    beginRemoveRows(QModelIndex(), row, row);
    m_subjects.removeAt(row);
    endRemoveRows();
    return true;
}

void TestSubjectListModel::insertSubject(int row, TestSubject subject)
{
    beginInsertRows(QModelIndex(), row, row);
    m_subjects.insert(row, subject);
    endInsertRows();
}

QJsonArray TestSubjectListModel::toJson()
{
    QJsonArray json;

    foreach(auto sub, m_subjects) {
        QJsonObject jsub;

        jsub["id"] = sub.id;
        jsub["group"] = sub.group;
        jsub["active"] = sub.active;
        jsub["adaptorHeight"] = sub.adaptorHeight;
        jsub["comment"] = sub.comment;

        json.append(jsub);
    }

    return json;
}

void TestSubjectListModel::fromJson(const QJsonArray &json)
{
    clear();

    beginInsertRows(QModelIndex(), 0, 0);
    foreach(auto jval, json) {
        auto jsub = jval.toObject();
        TestSubject sub;

        sub.id = jsub.value("id").toString();
        sub.group = jsub.value("group").toString();
        sub.active = jsub.value("active").toBool();
        sub.adaptorHeight = jsub.value("adaptorHeight").toInt();
        sub.comment = jsub.value("comment").toString();

        m_subjects.append(sub);
    }
    endInsertRows();
}

void TestSubjectListModel::clear()
{
    m_subjects.clear();
}
