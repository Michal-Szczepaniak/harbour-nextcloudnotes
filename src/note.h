#ifndef NOTE_H
#define NOTE_H

#include <QObject>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>

class Note : public QObject {
    Q_OBJECT

    Q_PROPERTY(int id READ id  WRITE setId  NOTIFY idChanged)
    Q_PROPERTY(uint modified READ modified WRITE setModified NOTIFY modifiedChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString category READ category WRITE setCategory NOTIFY categoryChanged)
    Q_PROPERTY(QString content READ content WRITE setContent NOTIFY contentChanged)
    Q_PROPERTY(bool favorite READ favorite WRITE setFavorite NOTIFY favoriteChanged)
    Q_PROPERTY(QString etag READ etag WRITE setEtag NOTIFY etagChanged)
    Q_PROPERTY(bool error READ error WRITE setError NOTIFY errorChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage WRITE setErrorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString dateString READ dateString NOTIFY dateStringChanged)

public:
    Note(QObject *parent = NULL);
    Note(const Note& note, QObject *parent = NULL);

    Note& operator=(const Note& note);
    bool operator==(const Note& note) const;
    bool same(const Note& note) const;
    bool same(const int id) const;
    bool equal(const Note& note) const;
    bool newer(const Note& note) const;
    bool older(const Note& note) const;

    enum SearchAttribute {
        NoSearchAttribute = 0x0,
        SearchInTitle = 0x1,
        SearchInCategory = 0x2,
        SearchInContent = 0x4,
        SearchAll = 0x7
    };
    Q_DECLARE_FLAGS(SearchAttributes, SearchAttribute)

    int id() const { return m_id; }
    uint modified() const { return m_modified; }
    QString title() const { return m_title; }
    QString category() const { return m_category; }
    QString content() const { return m_content; }
    bool favorite() const { return m_favorite; }
    QString etag() const { return m_etag; }
    bool error() const { return m_error; }
    QString errorMessage() const { return m_errorMessage; }
    QString dateString() const;

    void setId(int id) { if (id != m_id) { m_id = id; emit idChanged(id); } }
    void setModified(uint modified) { if (modified != m_modified) { m_modified = modified; emit modifiedChanged(modified); emit dateStringChanged(dateString()); } }
    void setTitle(QString title) { if (title != m_title) { m_title = title; emit titleChanged(title); } }
    void setCategory(QString category) { if (category != m_category) { m_category = category; emit categoryChanged(category); } }
    void setContent(QString content) { if (content != m_content) { m_content = content; emit contentChanged(content); } }
    void setFavorite(bool favorite) { if (favorite != m_favorite) { m_favorite = favorite; emit favoriteChanged(favorite); } }
    void setEtag(QString etag) { if (etag != m_etag) { m_etag = etag; emit etagChanged(etag); } }
    void setError(bool error) { if (error != m_error) { m_error = error; emit errorChanged(error); } }
    void setErrorMessage(QString errorMessage) { if (errorMessage != m_errorMessage) { m_errorMessage = errorMessage; emit errorMessageChanged(errorMessage); } }

    static Note fromjson(const QJsonObject& jobj);
    static bool searchInNote(const QString &query, const Note &note, SearchAttributes criteria = QFlag(SearchAll), Qt::CaseSensitivity cs = Qt::CaseInsensitive);
    static bool lessThanByDate(const Note &n1, const Note &n2);
    static bool lessThanByCategory(const Note &n1, const Note &n2);
    static bool lessThanByTitle(const Note &n1, const Note &n2);
    static bool lessThanByDateFavOnTop(const Note &n1, const Note &n2);
    static bool lessThanByCategoryFavOnTop(const Note &n1, const Note &n2);
    static bool lessThanByTitleFavOnTop(const Note &n1, const Note &n2);

signals:
    void idChanged(int id);
    void modifiedChanged(uint modified);
    void titleChanged(QString title);
    void categoryChanged(QString category);
    void contentChanged(QString content);
    void favoriteChanged(bool favorite);
    void etagChanged(QString etag);
    void errorChanged(bool error);
    void errorMessageChanged(QString errorMessage);
    void dateStringChanged(QString date);
    void noteChanged();

private:
    int m_id;
    uint m_modified;
    QString m_title;
    QString m_category;
    QString m_content;
    bool m_favorite;
    QString m_etag;
    bool m_error;
    QString m_errorMessage;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(Note::SearchAttributes)

#endif // NOTE_H