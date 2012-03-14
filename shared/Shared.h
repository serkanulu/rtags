#ifndef Shared_h
#define Shared_h

#include <QByteArray>
#include <clang-c/Index.h>
#include <Path.h>
#include <Log.h>
#include <stdio.h>

static inline int canonicalizePath(char *path, int len)
{
    Q_ASSERT(path[0] == '/');
    for (int i=0; i<len - 3; ++i) {
        if (path[i] == '/' && path[i + 1] == '.'
            && path[i + 2] == '.' && path[i + 3] == '/') {
            for (int j=i - 1; j>=0; --j) {
                if (path[j] == '/') {
                    memmove(path + j, path + i + 3, len - (i + 2));
                    const int removed = (i + 3 - j);
                    len -= removed;
                    i -= removed;
                    break;
                }
            }
        }
    }
    return len;
}

static inline int digits(int len)
{
    int ret = 1;
    while (len >= 10) {
        len /= 10;
        ++ret;
    }
    return ret;
}
static inline void makeLocation(QByteArray &path, int line, int col)
{
    const int size = path.size();
    const int extra = 2 + digits(line) + digits(col);
    path.resize(size + extra);
    snprintf(path.data() + size, extra + 1, ":%d:%d", line, col);
}

static inline QByteArray unescape(QByteArray command)
{
    command.replace('\'', "\\'");
    command.prepend("bash --norc -c 'echo -n ");
    command.append('\'');
    // QByteArray cmd = "bash --norc -c 'echo -n " + command + "'";
    FILE *f = popen(command.constData(), "r");
    QByteArray ret;
    char buf[1024];
    do {
        const int read = fread(buf, 1, 1024, f);
        if (read)
            ret += QByteArray::fromRawData(buf, read);
    } while (!feof(f));
    fclose(f);
    return ret;
}

static inline QByteArray eatString(CXString str)
{
    const QByteArray ret(clang_getCString(str));
    clang_disposeString(str);
    return ret;
}

static inline QByteArray join(const QList<QByteArray> &list, const QByteArray &sep = QByteArray())
{
    QByteArray ret;
    int size = qMax(0, list.size() - 1) * sep.size();
    foreach(const QByteArray &l, list) {
        size += l.size();
    }
    ret.reserve(size);
    foreach(const QByteArray &l, list) {
        ret.append(l);
    }
    return ret;
}

static inline QByteArray cursorToString(CXCursor cursor)
{
    QByteArray ret = eatString(clang_getCursorKindSpelling(clang_getCursorKind(cursor)));
    const QByteArray name = eatString(clang_getCursorSpelling(cursor));
    if (!name.isEmpty())
        ret += " " + name;

    CXFile file;
    unsigned line, col;
    clang_getInstantiationLocation(clang_getCursorLocation(cursor), &file, &line, &col, 0);
    const QByteArray fileName = eatString(clang_getFileName(file));
    if (!fileName.isEmpty()) {
        ret += " " + fileName + ':' + QByteArray::number(line) + ':' +  QByteArray::number(col);
    }
    return ret;
}

struct Location {
    Location() : line(0), column(0) {}

    Path path;
    int line, column;
};

static inline bool makeLocation(const QByteArray &arg, Location *loc,
                                QByteArray *resolvedLocation = 0, const Path &cwd = Path())
{
    Q_ASSERT(!arg.isEmpty());
    int colon = arg.lastIndexOf(':');
    if (colon == arg.size() - 1)
        colon = arg.lastIndexOf(':', colon - 1);
    if (colon == -1) {
        return false;
    }
    const unsigned column = atoi(arg.constData() + colon + 1);
    if (!column)
        return false;
    colon = arg.lastIndexOf(':', colon - 1);
    if (colon == -1)
        return false;
    const unsigned line = atoi(arg.constData() + colon + 1);
    if (!line)
        return false;
    const Path path = Path::resolved(arg.left(colon), cwd);
    if (path.isEmpty())
        return false;
    if (resolvedLocation)
        *resolvedLocation = path + arg.mid(colon);
    if (loc) {
        loc->line = line;
        loc->column = column;
        loc->path = path;
    }
    return true;
}


#endif
