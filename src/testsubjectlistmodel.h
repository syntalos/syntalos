#ifndef TESTSUBJECTLISTMODEL_H
#define TESTSUBJECTLISTMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QJsonArray>

struct TestSubject {
    QString id;
    QString group;
    bool active;
    int adaptorHeight; // in mm
    QString comment;
};

class TestSubjectListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit TestSubjectListModel(const QList<TestSubject>& subjects,
                                  QObject *parent = 0);
    explicit TestSubjectListModel(QObject *parent = 0);

    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;

    bool removeRows(int position, int rows, const QModelIndex &parent);
    bool removeRow(int row, const QModelIndex &parent = QModelIndex());

    void insertSubject(int row, TestSubject subject);
    void addSubject(const TestSubject subject);
    TestSubject subject(int row) const;

    QJsonArray toJson();
    void fromJson(const QJsonArray& json);

    void clear();

private:
    QList<TestSubject> m_subjects;
};

#endif // TESTSUBJECTLISTMODEL_H
