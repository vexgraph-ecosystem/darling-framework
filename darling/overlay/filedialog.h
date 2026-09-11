#ifndef DARLING_FILEDIALOG_H
#define DARLING_FILEDIALOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "oop/type.h"
#include "struct/list.h"


#define FILEDIALOG_PATH_MAX    512
#define FILEDIALOG_FILTER_MAX  64

// darling/overlay/filedialog.h — modal file browser with open/cancel hooks.

typedef void (*FileDialogOpenFn)(void *ctx);
typedef void (*FileDialogCancelFn)(void *ctx);

typedef struct FileDialog {
    Panel base;
    char path[512];
    char filter[64];
    bool showHidden;
    List *entries;
    int32_t selected;
    void (*onOpen)(void *ctx);
    void (*onCancel)(void *ctx);
    void *ctx;
} FileDialog;

// Constructors:
//   FileDialog()        — detached dialog at the empty path
//   FileDialog(parent)  — created and attached
FileDialog *FileDialog_0(void);
FileDialog *FileDialog_1(Panel *parent);

#define FileDialog(...) CONSTRUCTOR_DISPATCH(FileDialog, __VA_ARGS__)

// Core (shell stubs; directory scan lands with the io pass).
void FileDialog_refresh(FileDialog *d);
bool FileDialog_choose(FileDialog *d, int32_t index);

// Setters.
void FileDialog_setPath(FileDialog *d, const char *path);
void FileDialog_setFilter(FileDialog *d, const char *filter);
void FileDialog_setShowHidden(FileDialog *d, bool show);
void FileDialog_setSelected(FileDialog *d, int32_t index);
void FileDialog_setOnOpen(FileDialog *d, FileDialogOpenFn cb);
void FileDialog_setOnCancel(FileDialog *d, FileDialogCancelFn cb);
void FileDialog_setCtx(FileDialog *d, void *ctx);

// Getters.
const char *FileDialog_getPath(const FileDialog *d);
const char *FileDialog_getFilter(const FileDialog *d);
bool FileDialog_isShowHidden(const FileDialog *d);
int32_t FileDialog_getSelected(const FileDialog *d);
int32_t FileDialog_entryCount(const FileDialog *d);
const char *FileDialog_getEntryAt(const FileDialog *d, int32_t index);
FileDialogOpenFn FileDialog_getOnOpen(const FileDialog *d);
FileDialogCancelFn FileDialog_getOnCancel(const FileDialog *d);
void *FileDialog_getCtx(const FileDialog *d);

#endif
