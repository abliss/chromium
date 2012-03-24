// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_model.h"

#include "ash/launcher/launcher_model_observer.h"
#include "base/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// LauncherModelObserver implementation that tracks what message are invoked.
class TestLauncherModelObserver : public LauncherModelObserver {
 public:
  TestLauncherModelObserver()
      : added_count_(0),
        removed_count_(0),
        changed_count_(0),
        moved_count_(0) {
  }

  // Returns a string description of the changes that have occurred since this
  // was last invoked. Resets state to initial state.
  std::string StateStringAndClear() {
    std::string result;
    AddToResult("added=%d", added_count_, &result);
    AddToResult("removed=%d", removed_count_, &result);
    AddToResult("changed=%d", changed_count_, &result);
    AddToResult("moved=%d", moved_count_, &result);
    added_count_ = removed_count_ = changed_count_ = moved_count_ = 0;
    return result;
  }

  // LauncherModelObserver overrides:
  virtual void LauncherItemAdded(int index) OVERRIDE {
    added_count_++;
  }
  virtual void LauncherItemRemoved(int index, LauncherID id) OVERRIDE {
    removed_count_++;
  }
  virtual void LauncherItemChanged(int index,
                                   const LauncherItem& old_item) OVERRIDE {
    changed_count_++;
  }
  virtual void LauncherItemMoved(int start_index, int target_index) OVERRIDE {
    moved_count_++;
  }
  virtual void LauncherItemWillChange(int index) OVERRIDE {
  }

 private:
  void AddToResult(const std::string& format, int count, std::string* result) {
    if (!count)
      return;
    if (!result->empty())
      *result += " ";
    *result += base::StringPrintf(format.c_str(), count);
  }

  int added_count_;
  int removed_count_;
  int changed_count_;
  int moved_count_;

  DISALLOW_COPY_AND_ASSIGN(TestLauncherModelObserver);
};

}  // namespace

TEST(LauncherModel, BasicAssertions) {
  TestLauncherModelObserver observer;
  LauncherModel model;

  // Model is initially populated with two items.
  EXPECT_EQ(2, model.item_count());
  // Two initial items should have different ids.
  EXPECT_NE(model.items()[0].id, model.items()[1].id);

  // Add an item.
  model.AddObserver(&observer);
  LauncherItem item;
  int index = model.Add(item);
  EXPECT_EQ(3, model.item_count());
  EXPECT_EQ("added=1", observer.StateStringAndClear());
  // Verifies all the items get unique ids.
  std::set<LauncherID> ids;
  for (int i = 0; i < model.item_count(); ++i)
    ids.insert(model.items()[i].id);
  EXPECT_EQ(model.item_count(), static_cast<int>(ids.size()));

  // Change a tabbed image.
  LauncherID original_id = model.items()[index].id;
  model.Set(index, LauncherItem());
  EXPECT_EQ(original_id, model.items()[index].id);
  EXPECT_EQ("changed=1", observer.StateStringAndClear());
  EXPECT_EQ(TYPE_TABBED, model.items()[index].type);

  // Remove the item.
  model.RemoveItemAt(index);
  EXPECT_EQ(2, model.item_count());
  EXPECT_EQ("removed=1", observer.StateStringAndClear());

  // Add an app item.
  item.type = TYPE_APP;
  index = model.Add(item);
  observer.StateStringAndClear();

  // Change everything.
  model.Set(index, item);
  EXPECT_EQ("changed=1", observer.StateStringAndClear());
  EXPECT_EQ(TYPE_APP, model.items()[index].type);

  // Add another item.
  item.type = TYPE_APP;
  model.Add(item);
  observer.StateStringAndClear();

  // Move the third to the second.
  model.Move(2, 1);
  EXPECT_EQ("moved=1", observer.StateStringAndClear());

  // And back.
  model.Move(1, 2);
  EXPECT_EQ("moved=1", observer.StateStringAndClear());
}

// Assertions around where items are added.
TEST(LauncherModel, AddIndices) {
  TestLauncherModelObserver observer;
  LauncherModel model;

  // Model is initially populated with two items.
  EXPECT_EQ(2, model.item_count());
  // Two initial items should have different ids.
  EXPECT_NE(model.items()[0].id, model.items()[1].id);

  // Tabbed items should be after shortcut.
  LauncherItem item;
  int tabbed_index1 = model.Add(item);
  EXPECT_EQ(1, tabbed_index1);

  // Add another tabbed item, it should follow first.
  int tabbed_index2 = model.Add(item);
  EXPECT_EQ(2, tabbed_index2);

  // APP_SHORTCUT preceed browsers.
  item.type = TYPE_APP_SHORTCUT;
  int app_shortcut_index1 = model.Add(item);
  EXPECT_EQ(1, app_shortcut_index1);

  item.type = TYPE_APP_SHORTCUT;
  int app_shortcut_index2 = model.Add(item);
  EXPECT_EQ(2, app_shortcut_index2);

  // Apps should go with tabbed items.
  item.type = TYPE_APP;
  int app_index1 = model.Add(item);
  EXPECT_EQ(5, app_index1);

  EXPECT_EQ(TYPE_BROWSER_SHORTCUT, model.items()[0].type);
  EXPECT_EQ(TYPE_APP_LIST, model.items()[model.item_count() - 1].type);
}

}  // namespace ash
