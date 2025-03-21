// Copyright 2022 Signal Messenger, LLC
// SPDX-License-Identifier: AGPL-3.0-only

#ifndef CDS_PATH_ORAM_TREE_PATH_H
#define CDS_PATH_ORAM_TREE_PATH_H 1

#include "int_types.h"

typedef u64 tree_path[];

#define TREE_PATH_LENGTH(tp)  ((size_t)(tp)[0])
#define TREE_PATH_VALUES(tp)  (&(tp)[1])
/*
struct tree_path
{
  size_t length;
  u64 values[];
};
*/

tree_path *tree_path_create(u64 leaf, u64 root);
void tree_path_destroy(tree_path *tp);
void tree_path_update(tree_path *tp, u64 leaf);
size_t tree_path_num_nodes(size_t num_levels);
u64 tree_path_lower_bound(u64 val);
u64 tree_path_upper_bound(u64 val);
size_t tree_path_level(u64 val);

// jasmin functions
tree_path *tree_path_create_jazz(u64 leaf, u64 root);
void tree_path_update_jazz(tree_path *tp, u64 leaf);

void private_tree_path_tests();
#endif // CDS_PATH_ORAM_TREE_PATH_H
