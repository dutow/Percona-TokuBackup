/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ifndef BACKUP_DIRECTORY_H
#define BACKUP_DIRECTORY_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "description.h"
#include "fmap.h"
#include "copier.h"
#include "backup_callbacks.h"

#include <pthread.h>
#include <vector>
#include <pthread.h>

//////////////////////////////////////////////////////////////////////////////
//
class backup_session
{
public:
    backup_session(const char *source, const char *dest, backup_callbacks *call, file_hash_table * const table, int *errnum) throw(); // returns a nonzero in *errnum if an error callback has occured.
    ~backup_session() throw();
    int do_copy() throw() __attribute__((warn_unused_result)); // returns the error code (not in errno)
    int directories_set(backup_callbacks*) throw();
    bool is_prefix(const char *file) throw();
    bool is_prefix_of_realpath(const char *absfile) throw();

    char* translate_prefix(const char *file) throw();
    // Effect: Returns a malloc'd string which is the realpath of the filename translated from source directory to destination.

    char* translate_prefix_of_realpath(const char *absfile) throw();
    // Effect: Like translate_prefix, but requires that absfile is already the realpath of the file name.

    // Capture interface.
    int capture_open(const char *file, char **result) throw() __attribute__((warn_unused_result)); // if any errors occur, report them, and return the error code.  Otherwise return 0 and store the malloc'd name of the dest file  in *result.  If the file isn't in the destspace return 0 and set *result=NULL.
    int capture_mkdir(const char *pathname) throw() __attribute__((warn_unused_result)); // return 0 on success, error otherwise.
    void add_to_copy_todo_list(const char *file_path) throw();
    void cleanup(void) throw();
private:
    const char *m_source_dir;
    const char *m_dest_dir;
    copier m_copier;
};

#endif // End of header guardian.