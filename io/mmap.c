#include "io/mmap.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: MemoryMap
 * ============================================================================
 * Zero-copy read-only memory-mapped file utility: allocates address space
 * without physical RAM, paging blocks in from disk purely on demand via
 * CPU/GPU page faults. MemoryMap_open maps a file PROT_READ/MAP_PRIVATE and
 * closes the descriptor immediately — POSIX mmap retains its own inode
 * reference — returning a valid struct only on success.
 *
 * MemoryMap_close unmaps the region and resets the handle to its invalid
 * state. The handle is a plain value struct (data, size, valid) with no
 * arena or heap ownership of its own; the mapping is the OS resource.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: MemoryMap (read-only mapped file handle)
 * LEVEL: L2 — Behavior (memory-mapped file I/O behavior API)
 * ============================================================================
 * Zero-copy read-only memory mapped file utility: address space without
 * physical RAM, paged in on demand via CPU/GPU page faults.
 *
 * STRUCT FIELDS (Mirroring io/mmap.h):
 * ----------------------------------------------------------------------------
 *   void *data;            // Mapped base address (nullptr when invalid)
 *   size_t size;           // Mapped length in bytes (0 when invalid)
 *   bool valid;            // True once the mapping succeeded
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - MemoryMap_open(path)
 *   - MemoryMap_close(map)
 * ============================================================================
 */


MemoryMap MemoryMap_open(const char *path) {
    MemoryMap map = { .data = nullptr, .size = 0, .valid = false };
    if (!path) return map;

    int fd = open(path, O_RDONLY);
    if (fd < 0) return map;

    struct stat sb;
    if (fstat(fd, &sb) == 0 && sb.st_size > 0) {
        void *ptr = mmap(nullptr, (size_t)sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (ptr != MAP_FAILED) {
            map.data = ptr;
            map.size = (size_t)sb.st_size;
            map.valid = true;
        }
    }
    
    // POSIX mmap retains its own internal reference to the inode, 
    // so we can safely close the file descriptor immediately to avoid leaking.
    close(fd);
    return map;
}

void MemoryMap_close(MemoryMap *map) {
    if (map && (*map).valid && (*map).data && (*map).size > 0) {
        munmap((*map).data, (*map).size);
        (*map).data = nullptr;
        (*map).size = 0;
        (*map).valid = false;
    }
}
