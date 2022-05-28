/*
 * Copyright (C) 2015 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "mount-support.h"
#include "mount-support.c"
#include "mount-support-nvidia.h"
#include "mount-support-nvidia.c"

#include <glib.h>
#include <glib/gstdio.h>

static void replace_slashes_with_NUL(char *path, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if (path[i] == '/')
			path[i] = '\0';
	}
}

static void test_get_nextpath__typical(void)
{
	char path[] = "/some/path";
	size_t offset = 0;
	size_t fulllen = strlen(path);

	// Prepare path for usage with get_nextpath() by replacing
	// all path separators with the NUL byte.
	replace_slashes_with_NUL(path, fulllen);

	// Run get_nextpath a few times to see what happens.
	char *result;
	result = get_nextpath(path, &offset, fulllen);
	g_assert_cmpstr(result, ==, "some");
	result = get_nextpath(path, &offset, fulllen);
	g_assert_cmpstr(result, ==, "path");
	result = get_nextpath(path, &offset, fulllen);
	g_assert_cmpstr(result, ==, NULL);
}

static void test_get_nextpath__weird(void)
{
	char path[] = "..///path";
	size_t offset = 0;
	size_t fulllen = strlen(path);

	// Prepare path for usage with get_nextpath() by replacing
	// all path separators with the NUL byte.
	replace_slashes_with_NUL(path, fulllen);

	// Run get_nextpath a few times to see what happens.
	char *result;
	result = get_nextpath(path, &offset, fulllen);
	g_assert_cmpstr(result, ==, "path");
	result = get_nextpath(path, &offset, fulllen);
	g_assert_cmpstr(result, ==, NULL);
}

static void test_is_subdir(void)
{
	// Sensible exaples are sensible
	g_assert_true(is_subdir("/dir/subdir", "/dir/"));
	g_assert_true(is_subdir("/dir/subdir", "/dir"));
	g_assert_true(is_subdir("/dir/", "/dir"));
	g_assert_true(is_subdir("/dir", "/dir"));
	// Also without leading slash
	g_assert_true(is_subdir("dir/subdir", "dir/"));
	g_assert_true(is_subdir("dir/subdir", "dir"));
	g_assert_true(is_subdir("dir/", "dir"));
	g_assert_true(is_subdir("dir", "dir"));
	// Some more ideas
	g_assert_true(is_subdir("//", "/"));
	g_assert_true(is_subdir("/", "/"));
	g_assert_true(is_subdir("", ""));
	// but this is not true
	g_assert_false(is_subdir("/", "/dir"));
	g_assert_false(is_subdir("/rid", "/dir"));
	g_assert_false(is_subdir("/different/dir", "/dir"));
	g_assert_false(is_subdir("/", ""));
}

static void test_must_mkdir_and_open_with_perms(void)
{
	// make a directory with some contents and check we can
	// must_mkdir_and_open_with_perms() to get control of it
	GError *error = NULL;
	GStatBuf st;
	gchar *test_dir = g_dir_make_tmp("test-mkdir-XXXXXX", &error);
	g_assert_no_error(error);
	g_assert_nonnull(test_dir);
	g_assert_cmpint(chmod(test_dir, 0755), ==, 0);
	g_assert_true(g_file_test
		      (test_dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR));
	g_assert_cmpint(g_stat(test_dir, &st), ==, 0);
	g_assert_true(st.st_uid == getuid());
	g_assert_true(st.st_gid == getgid());
	g_assert_true(st.st_mode == (S_IFDIR | 0755));

	gchar *test_subdir = g_build_filename(test_dir, "foo", NULL);
	g_assert_cmpint(g_mkdir_with_parents(test_dir, 0755), ==, 0);
	g_file_test(test_subdir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR);

	// take over dir
	int fd =
	    must_mkdir_and_open_with_perms(test_dir, getuid(), getgid(), 0700);
	// check can unlink dir itself with no contents successfully and it
	// still exists
	g_assert_cmpint(fd, >=, 0);
	g_assert_false(g_file_test
		       (test_subdir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR));
	g_assert_true(g_file_test
		      (test_dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR));
	g_assert_cmpint(g_stat(test_dir, &st), ==, 0);
	g_assert_true(st.st_uid == getuid());
	g_assert_true(st.st_gid == getgid());
	g_assert_true(st.st_mode == (S_IFDIR | 0700));
	close(fd);
}

static void __attribute__((constructor)) init(void)
{
	g_test_add_func("/mount/get_nextpath/typical",
			test_get_nextpath__typical);
	g_test_add_func("/mount/get_nextpath/weird", test_get_nextpath__weird);
	g_test_add_func("/mount/is_subdir", test_is_subdir);
	g_test_add_func("/mount/must_mkdir_and_open_with_perms",
			test_must_mkdir_and_open_with_perms);
}
