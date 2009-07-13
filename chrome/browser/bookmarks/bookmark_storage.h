// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_STORAGE_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_STORAGE_H_

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/bookmarks/bookmark_index.h"
#include "chrome/common/important_file_writer.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class BookmarkModel;
class BookmarkNode;
class Profile;
class Task;
class Value;

namespace base {
class Thread;
}

// BookmarkStorage handles reading/write the bookmark bar model. The
// BookmarkModel uses the BookmarkStorage to load bookmarks from disk, as well
// as notifying the BookmarkStorage every time the model changes.
//
// Internally BookmarkStorage uses BookmarkCodec to do the actual read/write.
class BookmarkStorage : public NotificationObserver,
                        public ImportantFileWriter::DataSerializer,
                        public base::RefCountedThreadSafe<BookmarkStorage> {
 public:
  // LoadDetails is used by BookmarkStorage when loading bookmarks.
  // BookmarkModel creates a LoadDetails and passes it (including ownership) to
  // BookmarkStorage. BoomarkStorage loads the bookmarks (and index) in the
  // background thread, then calls back to the BookmarkModel (on the main
  // thread) when loading is done, passing ownership back to the BookmarkModel.
  // While loading BookmarkModel does not maintain references to the contents
  // of the LoadDetails, this ensures we don't have any threading problems.
  class LoadDetails {
   public:
    LoadDetails(BookmarkNode* bb_node,
                BookmarkNode* other_folder_node,
                BookmarkIndex* index,
                int64 max_id)
        : bb_node_(bb_node),
          other_folder_node_(other_folder_node),
          index_(index),
          max_id_(max_id),
          ids_reassigned_(false) {
    }

    void release() {
      bb_node_.release();
      other_folder_node_.release();
      index_.release();
    }

    BookmarkNode* bb_node() { return bb_node_.get(); }
    BookmarkNode* other_folder_node() { return other_folder_node_.get(); }
    BookmarkIndex* index() { return index_.get(); }

    // Max id of the nodes.
    void set_max_id(int64 max_id) { max_id_ = max_id; }
    int64 max_id() const { return max_id_; }

    // Computed checksum.
    void set_computed_checksum(const std::string& value) {
      computed_checksum_ = value;
    }
    const std::string& computed_checksum() const { return computed_checksum_; }

    // Stored checksum.
    void set_stored_checksum(const std::string& value) {
      stored_checksum_ = value;
    }
    const std::string& stored_checksum() const { return stored_checksum_; }

    // Whether ids were reassigned.
    void set_ids_reassigned(bool value) { ids_reassigned_ = value; }
    bool ids_reassigned() const { return ids_reassigned_; }

   private:
    scoped_ptr<BookmarkNode> bb_node_;
    scoped_ptr<BookmarkNode> other_folder_node_;
    scoped_ptr<BookmarkIndex> index_;
    int64 max_id_;
    std::string computed_checksum_;
    std::string stored_checksum_;
    bool ids_reassigned_;

    DISALLOW_COPY_AND_ASSIGN(LoadDetails);
  };

  // Creates a BookmarkStorage for the specified model
  BookmarkStorage(Profile* profile, BookmarkModel* model);
  ~BookmarkStorage();

  // Loads the bookmarks into the model, notifying the model when done. This
  // takes ownership of |details|. See LoadDetails for details.
  void LoadBookmarks(LoadDetails* details);

  // Schedules saving the bookmark bar model to disk.
  void ScheduleSave();

  // Notification the bookmark bar model is going to be deleted. If there is
  // a pending save, it is saved immediately.
  void BookmarkModelDeleted();

  // ImportantFileWriter::DataSerializer
  virtual bool SerializeData(std::string* output);

 private:
  class LoadTask;

  // Callback from backend with the results of the bookmark file.
  // This may be called multiple times, with different paths. This happens when
  // we migrate bookmark data from database.
  void OnLoadFinished(bool file_exists,
                      const FilePath& path);

  // Loads bookmark data from |file| and notifies the model when finished.
  void DoLoadBookmarks(const FilePath& file);

  // Load bookmarks data from the file written by history (StarredURLDatabase).
  void MigrateFromHistory();

  // Called when history has written the file with bookmarks data. Loads data
  // from that file.
  void OnHistoryFinishedWriting();

  // Called after we loaded file generated by history. Saves the data, deletes
  // the temporary file, and notifies the model.
  void FinishHistoryMigration();

  // NotificationObserver
  void Observe(NotificationType type, const NotificationSource& source,
               const NotificationDetails& details);

  // Serializes the data and schedules save using ImportantFileWriter.
  // Returns true on successful serialization.
  bool SaveNow();

  // Runs task on backend thread (or on current thread if backend thread
  // is NULL). Takes ownership of |task|.
  void RunTaskOnBackendThread(Task* task) const;

  // Returns the thread the backend is run on.
  const base::Thread* backend_thread() const { return backend_thread_; }

  // Keep the pointer to profile, we may need it for migration from history.
  Profile* profile_;

  // The model. The model is NULL once BookmarkModelDeleted has been invoked.
  BookmarkModel* model_;

  // Thread read/writing is run on. This comes from the profile, and is null
  // during testing.
  const base::Thread* backend_thread_;

  // Helper to write bookmark data safely.
  ImportantFileWriter writer_;

  // Helper to ensure that we unregister from notifications on destruction.
  NotificationRegistrar notification_registrar_;

  // Path to temporary file created during migrating bookmarks from history.
  const FilePath tmp_history_path_;

  // See class description of LoadDetails for details on this.
  scoped_ptr<LoadDetails> details_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkStorage);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_STORAGE_H_
