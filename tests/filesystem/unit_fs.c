#include "nth_test.h"

#include <narthex/narthex.h>
#include <narthex/nth_fs.h>
#include <narthex/nth_path.h>
#include <narthex/utils/platform.h>

#include <string.h>

#if NTH_PLATFORM_LINUX
#include <unistd.h> /* symlink() — fixture-only use, see test_query_dangling_symlink */
#endif


/* ================================================================================ */
/*  FIXTURE                                                                         */
/*                                                                                   */
/*  Each test gets its own subdirectory, named after itself, under the test         */
/*  binary's CWD (ctest runs from the build dir). Cleared at the start so a prior   */
/*  failed run doesn't leave stale state behind.                                   */
/* ================================================================================ */

static void fs_test_root(const char *name, NthPath *out) {
    nth_setup_path_cstr(out, "nth_fs_test_");
    nth_path_append(out, name);

    if(nth_fs_exists(nth_path_data(out)))
        nth_fs_remove_all(nth_path_data(out));

    nth_fs_create_dir_all(nth_path_data(out));
}

static void fs_test_child(const NthPath *root, const char *name, NthPath *out) {
    nth_setup_path_cstr(out, nth_path_data(root));
    nth_path_append(out, name);
}


/* ================================================================================ */
/*  QUERY                                                                           */
/* ================================================================================ */

static nth_b8 test_query_nonexistent(void) {
    NthPath root; fs_test_root("query_nonexistent", &root);
    NthPath p; fs_test_child(&root, "does_not_exist", &p);

    NTH_TEST_ASSERT(nth_fs_type(nth_path_data(&p)) == NTH_FS_ENTRY_NONE);
    NTH_TEST_ASSERT(!nth_fs_exists(nth_path_data(&p)));

    nth_teardown_path(&p);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}

static nth_b8 test_query_file(void) {
    NthPath root; fs_test_root("query_file", &root);
    NthPath p; fs_test_child(&root, "file.txt", &p);

    const nth_u8 data[] = "hello narthex";
    NTH_TEST_ASSERT(nth_fs_write_file(nth_path_data(&p), data, sizeof(data) - 1, NTH_TRUE) == NTH_RESULT_OK);

    NTH_TEST_ASSERT(nth_fs_type(nth_path_data(&p)) == NTH_FS_ENTRY_FILE);
    NTH_TEST_ASSERT(nth_fs_exists(nth_path_data(&p)));

    NthFsInfo info;
    NTH_TEST_ASSERT(nth_fs_stat(nth_path_data(&p), &info) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(info.type == NTH_FS_ENTRY_FILE);
    NTH_TEST_ASSERT(info.size == sizeof(data) - 1);

    nth_teardown_path(&p);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}

static nth_b8 test_query_directory(void) {
    NthPath root; fs_test_root("query_directory", &root);
    NthPath p; fs_test_child(&root, "subdir", &p);

    NTH_TEST_ASSERT(nth_fs_create_dir(nth_path_data(&p)) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_type(nth_path_data(&p)) == NTH_FS_ENTRY_DIRECTORY);

    NthFsInfo info;
    NTH_TEST_ASSERT(nth_fs_stat(nth_path_data(&p), &info) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(info.type == NTH_FS_ENTRY_DIRECTORY);

    nth_teardown_path(&p);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}

#if NTH_PLATFORM_LINUX
static nth_b8 test_query_dangling_symlink(void) {
    NthPath root; fs_test_root("query_dangling_symlink", &root);
    NthPath target; fs_test_child(&root, "does_not_exist", &target);
    NthPath link;   fs_test_child(&root, "the_link", &link);

    NTH_TEST_ASSERT(symlink(nth_path_data(&target), nth_path_data(&link)) == 0);

    NTH_TEST_ASSERT(nth_fs_type_link(nth_path_data(&link)) == NTH_FS_ENTRY_SYMLINK);
    NTH_TEST_ASSERT(nth_fs_exists_link(nth_path_data(&link)));
    NTH_TEST_ASSERT(nth_fs_type(nth_path_data(&link)) == NTH_FS_ENTRY_NONE); /* follow: dangling, nothing there */
    NTH_TEST_ASSERT(!nth_fs_exists(nth_path_data(&link)));

    NthFsInfo info;
    NTH_TEST_ASSERT(nth_fs_stat(nth_path_data(&link), &info) == NTH_RESULT_NOT_FOUND);
    NTH_TEST_ASSERT(nth_fs_stat_link(nth_path_data(&link), &info) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(info.type == NTH_FS_ENTRY_SYMLINK);

    nth_teardown_path(&link);
    nth_teardown_path(&target);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}
#endif


/* ================================================================================ */
/*  RESOLVE                                                                         */
/* ================================================================================ */

static nth_b8 test_resolve_plain_file(void) {
    NthPath root; fs_test_root("resolve_plain_file", &root);
    NthPath p; fs_test_child(&root, "file.txt", &p);

    const nth_u8 data[] = "x";
    NTH_TEST_ASSERT(nth_fs_write_file(nth_path_data(&p), data, 1, NTH_TRUE) == NTH_RESULT_OK);

    NthPath resolved;
    NTH_TEST_ASSERT(nth_setup_path(&resolved) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_resolve(nth_path_data(&p), &resolved) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_type(nth_path_data(&resolved)) == NTH_FS_ENTRY_FILE);

    nth_teardown_path(&resolved);
    nth_teardown_path(&p);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}

static nth_b8 test_resolve_nonexistent(void) {
    NthPath root; fs_test_root("resolve_nonexistent", &root);
    NthPath p; fs_test_child(&root, "nope", &p);

    NthPath resolved;
    NTH_TEST_ASSERT(nth_setup_path(&resolved) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_resolve(nth_path_data(&p), &resolved) == NTH_RESULT_NOT_FOUND);

    nth_teardown_path(&resolved);
    nth_teardown_path(&p);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}

#if NTH_PLATFORM_LINUX
static nth_b8 test_resolve_dangling_symlink(void) {
    NthPath root; fs_test_root("resolve_dangling_symlink", &root);
    NthPath target; fs_test_child(&root, "does_not_exist", &target);
    NthPath link;   fs_test_child(&root, "the_link", &link);

    NTH_TEST_ASSERT(symlink(nth_path_data(&target), nth_path_data(&link)) == 0);

    NthPath resolved;
    NTH_TEST_ASSERT(nth_setup_path(&resolved) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_resolve(nth_path_data(&link), &resolved) == NTH_RESULT_NOT_FOUND);

    nth_teardown_path(&resolved);
    nth_teardown_path(&link);
    nth_teardown_path(&target);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}
#endif


/* ================================================================================ */
/*  DIRECTORY ITERATION                                                            */
/* ================================================================================ */

static nth_b8 test_dir_empty(void) {
    NthPath root; fs_test_root("dir_empty", &root);

    NthFsDir dir;
    NTH_TEST_ASSERT(nth_setup_fs_dir(&dir, nth_path_data(&root)) == NTH_RESULT_OK);

    NthFsDirEntry entry;
    NTH_TEST_ASSERT(nth_fs_dir_next(&dir, &entry) == NTH_RESULT_NO_MORE_ITEMS);

    nth_teardown_fs_dir(&dir);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}

static nth_b8 test_dir_nonexistent(void) {
    NthPath root; fs_test_root("dir_nonexistent", &root);
    NthPath p; fs_test_child(&root, "nope", &p);

    NthFsDir dir;
    NTH_TEST_ASSERT(nth_setup_fs_dir(&dir, nth_path_data(&p)) == NTH_RESULT_NOT_FOUND);

    nth_teardown_path(&p);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}

static nth_b8 test_dir_lists_known_entries(void) {
    NthPath root; fs_test_root("dir_known_entries", &root);
    NthPath a; fs_test_child(&root, "a.txt", &a);
    NthPath b; fs_test_child(&root, "b.txt", &b);
    NthPath sub; fs_test_child(&root, "subdir", &sub);

    const nth_u8 one_byte[] = "x";
    NTH_TEST_ASSERT(nth_fs_write_file(nth_path_data(&a), one_byte, 1, NTH_TRUE) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_write_file(nth_path_data(&b), one_byte, 1, NTH_TRUE) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_create_dir(nth_path_data(&sub)) == NTH_RESULT_OK);

    nth_b8 seen_a = NTH_FALSE, seen_b = NTH_FALSE, seen_sub = NTH_FALSE;
    nth_usize count = 0;

    NthFsDir dir;
    NTH_TEST_ASSERT(nth_setup_fs_dir(&dir, nth_path_data(&root)) == NTH_RESULT_OK);

    NthFsDirEntry entry;
    NthResult step;
    while((step = nth_fs_dir_next(&dir, &entry)) == NTH_RESULT_OK) {
        count++;
        NTH_TEST_ASSERT(!nth_path_view_equal_cstr(entry.name, "."));
        NTH_TEST_ASSERT(!nth_path_view_equal_cstr(entry.name, ".."));

        if(nth_path_view_equal_cstr(entry.name, "a.txt")) { seen_a = NTH_TRUE; NTH_TEST_ASSERT(entry.type == NTH_FS_ENTRY_FILE); }
        if(nth_path_view_equal_cstr(entry.name, "b.txt")) { seen_b = NTH_TRUE; NTH_TEST_ASSERT(entry.type == NTH_FS_ENTRY_FILE); }
        if(nth_path_view_equal_cstr(entry.name, "subdir")) { seen_sub = NTH_TRUE; NTH_TEST_ASSERT(entry.type == NTH_FS_ENTRY_DIRECTORY); }
    }
    NTH_TEST_ASSERT(step == NTH_RESULT_NO_MORE_ITEMS);
    NTH_TEST_ASSERT(count == 3);
    NTH_TEST_ASSERT(seen_a && seen_b && seen_sub);

    nth_teardown_fs_dir(&dir);
    nth_teardown_path(&a); nth_teardown_path(&b); nth_teardown_path(&sub);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}


/* ================================================================================ */
/*  FILE I/O                                                                        */
/* ================================================================================ */

static nth_b8 test_file_write_read_round_trip(void) {
    NthPath root; fs_test_root("file_round_trip", &root);
    NthPath p; fs_test_child(&root, "data.bin", &p);

    const nth_u8 written[] = { 1, 2, 3, 4, 5, 255, 0, 128 };
    NTH_TEST_ASSERT(nth_fs_write_file(nth_path_data(&p), written, sizeof(written), NTH_TRUE) == NTH_RESULT_OK);

    nth_u8 read_buf[sizeof(written)];
    nth_usize read_size = 0;
    NTH_TEST_ASSERT(nth_fs_read_file(nth_path_data(&p), read_buf, sizeof(read_buf), &read_size) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(read_size == sizeof(written));
    NTH_TEST_ASSERT(memcmp(written, read_buf, sizeof(written)) == 0);

    nth_teardown_path(&p);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}

static nth_b8 test_file_write_no_overwrite_fails(void) {
    NthPath root; fs_test_root("file_no_overwrite", &root);
    NthPath p; fs_test_child(&root, "data.txt", &p);

    const nth_u8 data[] = "x";
    NTH_TEST_ASSERT(nth_fs_write_file(nth_path_data(&p), data, 1, NTH_TRUE) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_write_file(nth_path_data(&p), data, 1, NTH_FALSE) == NTH_RESULT_ALREADY_EXISTS);

    nth_teardown_path(&p);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}

static nth_b8 test_file_read_buffer_too_small(void) {
    NthPath root; fs_test_root("file_too_small", &root);
    NthPath p; fs_test_child(&root, "data.txt", &p);

    const nth_u8 data[] = "0123456789";
    NTH_TEST_ASSERT(nth_fs_write_file(nth_path_data(&p), data, sizeof(data) - 1, NTH_TRUE) == NTH_RESULT_OK);

    nth_u8 small_buf[4];
    nth_usize out_size = 0;
    NTH_TEST_ASSERT(nth_fs_read_file(nth_path_data(&p), small_buf, sizeof(small_buf), &out_size) == NTH_RESULT_CAPACITY_FULL);
    NTH_TEST_ASSERT(out_size == sizeof(data) - 1); /* real size reported even on failure */

    nth_teardown_path(&p);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}

static nth_b8 test_file_seek_and_tell(void) {
    NthPath root; fs_test_root("file_seek_tell", &root);
    NthPath p; fs_test_child(&root, "data.bin", &p);

    NthFsFile f;
    NTH_TEST_ASSERT(nth_setup_fs_file(&f, nth_path_data(&p), NTH_FS_OPEN_READ_WRITE) == NTH_RESULT_OK);

    const nth_u8 payload[] = "abcdefgh";
    nth_usize written = 0;
    NTH_TEST_ASSERT(nth_fs_file_write(&f, payload, sizeof(payload) - 1, &written) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(written == sizeof(payload) - 1);
    NTH_TEST_ASSERT(nth_fs_file_flush(&f) == NTH_RESULT_OK);

    NTH_TEST_ASSERT(nth_fs_file_seek(&f, 0, NTH_FS_SEEK_END) == NTH_RESULT_OK);
    nth_u64 pos = 0;
    NTH_TEST_ASSERT(nth_fs_file_tell(&f, &pos) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(pos == sizeof(payload) - 1);

    NTH_TEST_ASSERT(nth_fs_file_seek(&f, 3, NTH_FS_SEEK_START) == NTH_RESULT_OK);
    nth_u8 read_buf[4] = {0};
    nth_usize read_n = 0;
    NTH_TEST_ASSERT(nth_fs_file_read(&f, read_buf, 4, &read_n) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(read_n == 4);
    NTH_TEST_ASSERT(memcmp(read_buf, "defg", 4) == 0);

    nth_teardown_fs_file(&f);
    nth_teardown_path(&p);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}

static nth_b8 test_file_read_nonexistent(void) {
    NthPath root; fs_test_root("file_read_nonexistent", &root);
    NthPath p; fs_test_child(&root, "nope.txt", &p);

    nth_u8 buf[8];
    nth_usize out_size = 0;
    NTH_TEST_ASSERT(nth_fs_read_file(nth_path_data(&p), buf, sizeof(buf), &out_size) == NTH_RESULT_NOT_FOUND);

    nth_teardown_path(&p);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}


/* ================================================================================ */
/*  CREATE / REMOVE / RENAME                                                        */
/* ================================================================================ */

static nth_b8 test_create_dir_all_nested(void) {
    NthPath root; fs_test_root("create_dir_all_nested", &root);
    NthPath deep; fs_test_child(&root, "a", &deep);
    nth_path_append(&deep, "b");
    nth_path_append(&deep, "c");

    NTH_TEST_ASSERT(nth_fs_create_dir_all(nth_path_data(&deep)) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_type(nth_path_data(&deep)) == NTH_FS_ENTRY_DIRECTORY);

    NthPath mid; fs_test_child(&root, "a", &mid);
    nth_path_append(&mid, "b");
    NTH_TEST_ASSERT(nth_fs_type(nth_path_data(&mid)) == NTH_FS_ENTRY_DIRECTORY);

    nth_teardown_path(&mid);
    nth_teardown_path(&deep);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}

static nth_b8 test_create_dir_all_tolerates_existing(void) {
    NthPath root; fs_test_root("create_dir_all_existing", &root);

    NTH_TEST_ASSERT(nth_fs_create_dir_all(nth_path_data(&root)) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_create_dir_all(nth_path_data(&root)) == NTH_RESULT_OK);

    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}

static nth_b8 test_create_dir_missing_parent_fails(void) {
    NthPath root; fs_test_root("create_dir_missing_parent", &root);
    NthPath p; fs_test_child(&root, "a", &p);
    nth_path_append(&p, "b"); /* "a" doesn't exist — single-level create_dir must not make it */

    NTH_TEST_ASSERT(nth_fs_create_dir(nth_path_data(&p)) == NTH_RESULT_NOT_FOUND);

    nth_teardown_path(&p);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}

static nth_b8 test_remove_file(void) {
    NthPath root; fs_test_root("remove_file", &root);
    NthPath p; fs_test_child(&root, "file.txt", &p);

    const nth_u8 data[] = "x";
    NTH_TEST_ASSERT(nth_fs_write_file(nth_path_data(&p), data, 1, NTH_TRUE) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_remove(nth_path_data(&p)) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(!nth_fs_exists(nth_path_data(&p)));

    nth_teardown_path(&p);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}

static nth_b8 test_remove_nonempty_dir_fails(void) {
    NthPath root; fs_test_root("remove_nonempty", &root);
    NthPath p; fs_test_child(&root, "file.txt", &p);

    const nth_u8 data[] = "x";
    NTH_TEST_ASSERT(nth_fs_write_file(nth_path_data(&p), data, 1, NTH_TRUE) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_remove(nth_path_data(&root)) == NTH_RESULT_INVALID_STATE);

    nth_teardown_path(&p);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}

static nth_b8 test_remove_all_clears_tree(void) {
    NthPath root; fs_test_root("remove_all_tree", &root);
    NthPath sub; fs_test_child(&root, "sub", &sub);
    NthPath f1; fs_test_child(&root, "f1.txt", &f1);
    NthPath f2;
    nth_setup_path_cstr(&f2, nth_path_data(&sub));
    nth_path_append(&f2, "f2.txt");

    NTH_TEST_ASSERT(nth_fs_create_dir(nth_path_data(&sub)) == NTH_RESULT_OK);
    const nth_u8 data[] = "x";
    NTH_TEST_ASSERT(nth_fs_write_file(nth_path_data(&f1), data, 1, NTH_TRUE) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_write_file(nth_path_data(&f2), data, 1, NTH_TRUE) == NTH_RESULT_OK);

    NTH_TEST_ASSERT(nth_fs_remove_all(nth_path_data(&root)) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(!nth_fs_exists(nth_path_data(&root)));

    nth_teardown_path(&f2); nth_teardown_path(&f1); nth_teardown_path(&sub);
    nth_teardown_path(&root);
    return NTH_TRUE;
}

static nth_b8 test_remove_all_on_file_fails(void) {
    NthPath root; fs_test_root("remove_all_on_file", &root);
    NthPath p; fs_test_child(&root, "file.txt", &p);

    const nth_u8 data[] = "x";
    NTH_TEST_ASSERT(nth_fs_write_file(nth_path_data(&p), data, 1, NTH_TRUE) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_remove_all(nth_path_data(&p)) == NTH_RESULT_NOT_FOUND);

    nth_teardown_path(&p);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}

#if NTH_PLATFORM_LINUX
static nth_b8 test_remove_all_refuses_symlink(void) {
    NthPath root; fs_test_root("remove_all_refuses_symlink", &root);
    NthPath target; fs_test_child(&root, "real_dir", &target);
    NthPath link;    fs_test_child(&root, "the_link", &link);

    NTH_TEST_ASSERT(nth_fs_create_dir(nth_path_data(&target)) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(symlink(nth_path_data(&target), nth_path_data(&link)) == 0);

    NTH_TEST_ASSERT(nth_fs_remove_all(nth_path_data(&link)) == NTH_RESULT_INVALID_ARGUMENT);
    NTH_TEST_ASSERT(nth_fs_exists_link(nth_path_data(&link))); /* untouched */

    nth_teardown_path(&link);
    nth_teardown_path(&target);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}
#endif

static nth_b8 test_rename_basic(void) {
    NthPath root; fs_test_root("rename_basic", &root);
    NthPath from; fs_test_child(&root, "old.txt", &from);
    NthPath to;   fs_test_child(&root, "new.txt", &to);

    const nth_u8 data[] = "payload";
    NTH_TEST_ASSERT(nth_fs_write_file(nth_path_data(&from), data, sizeof(data) - 1, NTH_TRUE) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_rename(nth_path_data(&from), nth_path_data(&to)) == NTH_RESULT_OK);

    NTH_TEST_ASSERT(!nth_fs_exists(nth_path_data(&from)));
    NTH_TEST_ASSERT(nth_fs_exists(nth_path_data(&to)));

    nth_u8 buf[16]; nth_usize out_size = 0;
    NTH_TEST_ASSERT(nth_fs_read_file(nth_path_data(&to), buf, sizeof(buf), &out_size) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(out_size == sizeof(data) - 1);
    NTH_TEST_ASSERT(memcmp(buf, data, out_size) == 0);

    nth_teardown_path(&to); nth_teardown_path(&from);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}

static nth_b8 test_rename_replaces_existing_destination(void) {
    /* Windows' native rename fails if the destination exists, unlike POSIX.
       nth_fs_rename must behave the same on both — this is the test that
       catches a regression back to plain MoveFileA/CRT rename() on Windows. */
    NthPath root; fs_test_root("rename_replaces_existing", &root);
    NthPath from; fs_test_child(&root, "from.txt", &from);
    NthPath to;   fs_test_child(&root, "to.txt", &to);

    const nth_u8 old_data[] = "old";
    const nth_u8 new_data[] = "new content";
    NTH_TEST_ASSERT(nth_fs_write_file(nth_path_data(&to), old_data, sizeof(old_data) - 1, NTH_TRUE) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_write_file(nth_path_data(&from), new_data, sizeof(new_data) - 1, NTH_TRUE) == NTH_RESULT_OK);

    NTH_TEST_ASSERT(nth_fs_rename(nth_path_data(&from), nth_path_data(&to)) == NTH_RESULT_OK);

    nth_u8 buf[32]; nth_usize out_size = 0;
    NTH_TEST_ASSERT(nth_fs_read_file(nth_path_data(&to), buf, sizeof(buf), &out_size) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(out_size == sizeof(new_data) - 1);
    NTH_TEST_ASSERT(memcmp(buf, new_data, out_size) == 0);

    nth_teardown_path(&to); nth_teardown_path(&from);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}


/* ================================================================================ */
/*  SYSTEM PATHS                                                                    */
/* ================================================================================ */

static nth_b8 test_system_cwd_exists(void) {
    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path(&p) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_system_cwd(&p) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_type(nth_path_data(&p)) == NTH_FS_ENTRY_DIRECTORY);
    nth_teardown_path(&p);
    return NTH_TRUE;
}

static nth_b8 test_system_cwd_stable_across_calls(void) {
    NthPath a, b;
    NTH_TEST_ASSERT(nth_setup_path(&a) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_setup_path(&b) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_system_cwd(&a) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_system_cwd(&b) == NTH_RESULT_OK); /* exercises the cached path */
    NTH_TEST_ASSERT(nth_path_equal(&a, &b));
    nth_teardown_path(&a); nth_teardown_path(&b);
    return NTH_TRUE;
}

static nth_b8 test_system_exe_is_a_directory(void) {
    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path(&p) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_system_exe(&p) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_type(nth_path_data(&p)) == NTH_FS_ENTRY_DIRECTORY);
    nth_teardown_path(&p);
    return NTH_TRUE;
}

static nth_b8 test_system_temp_exists(void) {
    NthPath p;
    NTH_TEST_ASSERT(nth_setup_path(&p) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_system_temp(&p) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_type(nth_path_data(&p)) == NTH_FS_ENTRY_DIRECTORY);
    nth_teardown_path(&p);
    return NTH_TRUE;
}

static nth_b8 test_system_config_differs_by_app_name(void) {
    NthPath no_app, with_app;
    NTH_TEST_ASSERT(nth_setup_path(&no_app) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_setup_path(&with_app) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_system_config(NULL, &no_app) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_system_config("nth_test_app", &with_app) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(!nth_path_equal(&no_app, &with_app));
    nth_teardown_path(&no_app); nth_teardown_path(&with_app);
    return NTH_TRUE;
}

#if NTH_PLATFORM_LINUX
static nth_b8 test_symlink_to_directory_round_trip(void) {
    NthPath root; fs_test_root("symlink_dir_roundtrip", &root);
    NthPath target; fs_test_child(&root, "real_dir", &target);
    NthPath link;    fs_test_child(&root, "the_link", &link);

    NTH_TEST_ASSERT(nth_fs_create_dir(nth_path_data(&target)) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_create_symlink(nth_path_data(&target), nth_path_data(&link), NTH_FS_ENTRY_DIRECTORY) == NTH_RESULT_OK);
    NTH_TEST_ASSERT(nth_fs_type_link(nth_path_data(&link)) == NTH_FS_ENTRY_SYMLINK);

    NTH_TEST_ASSERT(nth_fs_remove(nth_path_data(&link)) == NTH_RESULT_OK); /* must remove the link, not the target */
    NTH_TEST_ASSERT(nth_fs_exists(nth_path_data(&target))); /* target survives */
    NTH_TEST_ASSERT(!nth_fs_exists_link(nth_path_data(&link)));

    nth_teardown_path(&link); nth_teardown_path(&target);
    nth_fs_remove_all(nth_path_data(&root));
    nth_teardown_path(&root);
    return NTH_TRUE;
}
#endif


int main(void) {
    NthTest tests[] = {
        { "query/nonexistent",                     test_query_nonexistent },
        { "query/file",                             test_query_file },
        { "query/directory",                        test_query_directory },
#if NTH_PLATFORM_LINUX
        { "query/dangling_symlink",                 test_query_dangling_symlink },
#endif
        { "resolve/plain_file",                     test_resolve_plain_file },
        { "resolve/nonexistent",                    test_resolve_nonexistent },
#if NTH_PLATFORM_LINUX
        { "resolve/dangling_symlink",                test_resolve_dangling_symlink },
#endif
        { "dir/empty",                               test_dir_empty },
        { "dir/nonexistent",                         test_dir_nonexistent },
        { "dir/lists_known_entries",                 test_dir_lists_known_entries },
        { "file/write_read_round_trip",              test_file_write_read_round_trip },
        { "file/no_overwrite_fails",                 test_file_write_no_overwrite_fails },
        { "file/read_buffer_too_small",               test_file_read_buffer_too_small },
        { "file/seek_and_tell",                       test_file_seek_and_tell },
        { "file/read_nonexistent",                    test_file_read_nonexistent },
        { "create/dir_all_nested",                    test_create_dir_all_nested },
        { "create/dir_all_tolerates_existing",        test_create_dir_all_tolerates_existing },
        { "create/dir_missing_parent_fails",          test_create_dir_missing_parent_fails },
        { "remove/file",                              test_remove_file },
        { "remove/nonempty_dir_fails",                test_remove_nonempty_dir_fails },
        { "remove/all_clears_tree",                   test_remove_all_clears_tree },
        { "remove/all_on_file_fails",                 test_remove_all_on_file_fails },
#if NTH_PLATFORM_LINUX
        { "remove/all_refuses_symlink",               test_remove_all_refuses_symlink },
        { "no_idea/symlink_to_directory_round_trip", test_symlink_to_directory_round_trip},
#endif
        { "rename/basic",                             test_rename_basic },
        { "rename/replaces_existing_destination",     test_rename_replaces_existing_destination },
        { "system/cwd_exists",                        test_system_cwd_exists },
        { "system/cwd_stable",                        test_system_cwd_stable_across_calls },
        { "system/exe_is_directory",                  test_system_exe_is_a_directory },
        { "system/temp_exists",                       test_system_temp_exists },
        { "system/config_differs_by_app_name",        test_system_config_differs_by_app_name },
    };

    return NTH_RUN_TESTS(tests);
}