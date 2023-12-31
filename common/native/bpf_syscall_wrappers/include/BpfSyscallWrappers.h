/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <linux/bpf.h>
#include <linux/unistd.h>

#ifdef BPF_FD_JUST_USE_INT
  #define BPF_FD_TYPE int
  #define BPF_FD_TO_U32(x) static_cast<__u32>(x)
#else
  #include <android-base/unique_fd.h>
  #define BPF_FD_TYPE base::unique_fd&
  #define BPF_FD_TO_U32(x) static_cast<__u32>((x).get())
#endif

#define ptr_to_u64(x) ((uint64_t)(uintptr_t)(x))

namespace android {
namespace bpf {

/* Note: bpf_attr is a union which might have a much larger size then the anonymous struct portion
 * of it that we are using.  The kernel's bpf() system call will perform a strict check to ensure
 * all unused portions are zero.  It will fail with E2BIG if we don't fully zero bpf_attr.
 */

inline int bpf(enum bpf_cmd cmd, const bpf_attr& attr) {
    return syscall(__NR_bpf, cmd, &attr, sizeof(attr));
}

inline int createMap(bpf_map_type map_type, uint32_t key_size, uint32_t value_size,
                     uint32_t max_entries, uint32_t map_flags) {
    return bpf(BPF_MAP_CREATE, {
                                       .map_type = map_type,
                                       .key_size = key_size,
                                       .value_size = value_size,
                                       .max_entries = max_entries,
                                       .map_flags = map_flags,
                               });
}

inline int writeToMapEntry(const BPF_FD_TYPE map_fd, const void* key, const void* value,
                           uint64_t flags) {
    return bpf(BPF_MAP_UPDATE_ELEM, {
                                            .map_fd = BPF_FD_TO_U32(map_fd),
                                            .key = ptr_to_u64(key),
                                            .value = ptr_to_u64(value),
                                            .flags = flags,
                                    });
}

inline int findMapEntry(const BPF_FD_TYPE map_fd, const void* key, void* value) {
    return bpf(BPF_MAP_LOOKUP_ELEM, {
                                            .map_fd = BPF_FD_TO_U32(map_fd),
                                            .key = ptr_to_u64(key),
                                            .value = ptr_to_u64(value),
                                    });
}

inline int deleteMapEntry(const BPF_FD_TYPE map_fd, const void* key) {
    return bpf(BPF_MAP_DELETE_ELEM, {
                                            .map_fd = BPF_FD_TO_U32(map_fd),
                                            .key = ptr_to_u64(key),
                                    });
}

inline int getNextMapKey(const BPF_FD_TYPE map_fd, const void* key, void* next_key) {
    return bpf(BPF_MAP_GET_NEXT_KEY, {
                                             .map_fd = BPF_FD_TO_U32(map_fd),
                                             .key = ptr_to_u64(key),
                                             .next_key = ptr_to_u64(next_key),
                                     });
}

inline int getFirstMapKey(const BPF_FD_TYPE map_fd, void* firstKey) {
    return getNextMapKey(map_fd, NULL, firstKey);
}

inline int bpfFdPin(const BPF_FD_TYPE map_fd, const char* pathname) {
    return bpf(BPF_OBJ_PIN, {
                                    .pathname = ptr_to_u64(pathname),
                                    .bpf_fd = BPF_FD_TO_U32(map_fd),
                            });
}

inline int bpfFdGet(const char* pathname, uint32_t flag) {
    return bpf(BPF_OBJ_GET, {
                                    .pathname = ptr_to_u64(pathname),
                                    .file_flags = flag,
                            });
}

inline int mapRetrieve(const char* pathname, uint32_t flag) {
    return bpfFdGet(pathname, flag);
}

inline int mapRetrieveRW(const char* pathname) {
    return mapRetrieve(pathname, 0);
}

inline int mapRetrieveRO(const char* pathname) {
    return mapRetrieve(pathname, BPF_F_RDONLY);
}

inline int mapRetrieveWO(const char* pathname) {
    return mapRetrieve(pathname, BPF_F_WRONLY);
}

inline int retrieveProgram(const char* pathname) {
    return bpfFdGet(pathname, BPF_F_RDONLY);
}

inline int attachProgram(bpf_attach_type type, const BPF_FD_TYPE prog_fd,
                         const BPF_FD_TYPE cg_fd, uint32_t flags = 0) {
    return bpf(BPF_PROG_ATTACH, {
                                        .target_fd = BPF_FD_TO_U32(cg_fd),
                                        .attach_bpf_fd = BPF_FD_TO_U32(prog_fd),
                                        .attach_type = type,
                                        .attach_flags = flags,
                                });
}

inline int detachProgram(bpf_attach_type type, const BPF_FD_TYPE cg_fd) {
    return bpf(BPF_PROG_DETACH, {
                                        .target_fd = BPF_FD_TO_U32(cg_fd),
                                        .attach_type = type,
                                });
}

inline int detachSingleProgram(bpf_attach_type type, const BPF_FD_TYPE prog_fd,
                               const BPF_FD_TYPE cg_fd) {
    return bpf(BPF_PROG_DETACH, {
                                        .target_fd = BPF_FD_TO_U32(cg_fd),
                                        .attach_bpf_fd = BPF_FD_TO_U32(prog_fd),
                                        .attach_type = type,
                                });
}

// Available in 4.12 and later kernels.
inline int runProgram(const BPF_FD_TYPE prog_fd, const void* data,
                      const uint32_t data_size) {
    return bpf(BPF_PROG_RUN, {
                                     .test = {
                                             .prog_fd = BPF_FD_TO_U32(prog_fd),
                                             .data_in = ptr_to_u64(data),
                                             .data_size_in = data_size,
                                     },
                             });
}

// BPF_OBJ_GET_INFO_BY_FD requires 4.14+ kernel
//
// Note: some fields are only defined in newer kernels (ie. the map_info struct grows
// over time), so we need to check that the field we're interested in is actually
// supported/returned by the running kernel.  We do this by checking it is fully
// within the bounds of the struct size as reported by the kernel.
#define DEFINE_BPF_GET_FD_INFO(NAME, FIELD) \
inline int bpfGetFd ## NAME(const BPF_FD_TYPE map_fd) { \
    struct bpf_map_info map_info = {}; \
    union bpf_attr attr = { .info = { \
        .bpf_fd = BPF_FD_TO_U32(map_fd), \
        .info_len = sizeof(map_info), \
        .info = ptr_to_u64(&map_info), \
    }}; \
    int rv = bpf(BPF_OBJ_GET_INFO_BY_FD, attr); \
    if (rv) return rv; \
    if (attr.info.info_len < offsetof(bpf_map_info, FIELD) + sizeof(map_info.FIELD)) { \
        errno = EOPNOTSUPP; \
        return -1; \
    }; \
    return map_info.FIELD; \
}

// All 6 of these fields are already present in Linux v4.14 (even ACK 4.14-P)
// while BPF_OBJ_GET_INFO_BY_FD is not implemented at all in v4.9 (even ACK 4.9-Q)
DEFINE_BPF_GET_FD_INFO(MapType, type)            // int bpfGetFdMapType(const BPF_FD_TYPE map_fd)
DEFINE_BPF_GET_FD_INFO(MapId, id)                // int bpfGetFdMapId(const BPF_FD_TYPE map_fd)
DEFINE_BPF_GET_FD_INFO(KeySize, key_size)        // int bpfGetFdKeySize(const BPF_FD_TYPE map_fd)
DEFINE_BPF_GET_FD_INFO(ValueSize, value_size)    // int bpfGetFdValueSize(const BPF_FD_TYPE map_fd)
DEFINE_BPF_GET_FD_INFO(MaxEntries, max_entries)  // int bpfGetFdMaxEntries(const BPF_FD_TYPE map_fd)
DEFINE_BPF_GET_FD_INFO(MapFlags, map_flags)      // int bpfGetFdMapFlags(const BPF_FD_TYPE map_fd)

#undef DEFINE_BPF_GET_FD_INFO

}  // namespace bpf
}  // namespace android

#undef ptr_to_u64
#undef BPF_FD_TO_U32
#undef BPF_FD_TYPE
#undef BPF_FD_JUST_USE_INT
