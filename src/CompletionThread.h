/* This file is part of RTags (http://rtags.net).

   RTags is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   RTags is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with RTags.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef CompletionThread_h
#define CompletionThread_h

#include <clang-c/Index.h>
#include <condition_variable>
#include <memory>
#include <mutex>

#include "Location.h"
#include "rct/Connection.h"
#include "rct/EmbeddedLinkedList.h"
#include "rct/Flags.h"
#include "rct/LinkedList.h"
#include "rct/Map.h"
#include "rct/Thread.h"
#include "Source.h"
#include "RTags.h"

class CompletionThread : public Thread
{
public:
    CompletionThread(int cacheSize);
    ~CompletionThread();

    virtual void run() override;
    enum Flag {
        None = 0x00,
        Refresh = 0x01,
        Elisp = 0x02,
        XML = 0x04,
        JSON = 0x08,
        CodeCompleteIncludeMacros = 0x10,
        WarmUp = 0x20
    };
    bool isCached(uint32_t fileId, const std::shared_ptr<Project> &project) const;
    void completeAt(Source &&source, Location location, Flags<Flag> flags,
                    String &&unsaved, const std::shared_ptr<Connection> &conn);
    void prepare(Source &&source, String &&unsaved);
    void stop();
    String dump();
private:
    struct Request;
    void process(Request *request);

    Set<uint32_t> mWatched;
    bool mShutdown;
    const size_t mCacheSize;
    struct Request {
        ~Request()
        {
            if (conn)
                conn->finish();
        }
        Source source;
        Location location;
        Flags<Flag> flags;
        String unsaved;
        std::shared_ptr<Connection> conn;
    };
    LinkedList<Request*> mPending;
    struct Dump {
        bool done;
        std::mutex mutex;
        std::condition_variable cond;
        String string;
    } *mDump;
    CXIndex mIndex;

    struct Completions {
        Completions(Location loc) : location(loc), next(0), prev(0) {}
        struct Candidate {
            String completion, signature, annotation, parent, briefComment;
            int priority, distance;
            CXCursorKind cursorKind;
        };
        List<Candidate> candidates;
        const Location location;
        Completions *next, *prev;
    };

    void printCompletions(const List<Completions::Candidate> &completions, Request *request);
    static bool compareCompletionCandidates(const Completions::Candidate *l,
                                            const Completions::Candidate *r);

    struct SourceFile {
        SourceFile()
            : translationUnit(0), unsavedHash(0), lastModified(0),
              parseTime(0), reparseTime(0), codeCompleteTime(0), completions(0), next(0), prev(0)
        {}
        std::shared_ptr<RTags::TranslationUnit> translationUnit;
        size_t unsavedHash;
        uint64_t lastModified, parseTime, reparseTime, codeCompleteTime; // ms
        size_t completions;
        Source source;
        Map<Location, Completions*> completionsMap;
        EmbeddedLinkedList<Completions*> completionsList;
        SourceFile *next, *prev;
    };

    struct Token
    {
        Token(const char *bytes = 0, int size = 0)
            : data(bytes), length(size)
        {}

        inline bool operator==(const Token &other) const
        {
            return length == other.length && !strncmp(data, other.data, length);
        }
        inline bool operator<(const Token &other) const
        {
            if (!data)
                return !other.data ? 0 : -1;
            if (!other.data)
                return 1;
            const int minLength = std::min(length, other.length);
            int ret = memcmp(data, other.data, minLength);
            if (!ret) {
                if (length < other.length) {
                    ret = -1;
                } else if (other.length < length) {
                    ret = 1;
                }
            }
            return ret;
        }

        const char *data;
        int length;

        static inline Map<Token, int> tokenize(const char *data, int size)
        {
            Map<Token, int> tokens;
            int tokenEnd = -1;
            for (int i=size - 1; i>=0; --i) {
                if (RTags::isSymbol(data[i])) {
                    if (tokenEnd == -1)
                        tokenEnd = i;
                } else if (tokenEnd != -1) {
                    addToken(data, i + 1, tokenEnd - i, tokens);
                    tokenEnd = -1;
                }
            }
            if (tokenEnd != -1)
                addToken(data, 0, tokenEnd + 1, tokens);
            return tokens;
        }
    private:
        static inline void addToken(const char *data, int pos, int len, Map<Token, int> &tokens)
        {
            int &val = tokens[Token(data + pos, len)];
            if (!val)
                val = pos;
        }
    };


    // these datastructures are only touched from inside the thread so it doesn't
    // need to be protected by mMutex
    Hash<uint32_t, SourceFile*> mCacheMap;
    EmbeddedLinkedList<SourceFile*> mCacheList;

    mutable std::mutex mMutex;
    std::condition_variable mCondition;
};

RCT_FLAGS(CompletionThread::Flag);

#endif
