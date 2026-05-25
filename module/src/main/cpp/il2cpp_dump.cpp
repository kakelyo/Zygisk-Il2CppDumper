//
// Created by Perfare on 2020/7/4.
//

#include "il2cpp_dump.h"
#include "script_json.h"
#include "il2cpp_h.h"
#include <dlfcn.h>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <unistd.h>
#include "xdl.h"
#include "log.h"
#include "il2cpp-tabledefs.h"
#include "il2cpp-class.h"

#define DO_API(r, n, p) r (*n) p

#include "il2cpp-api-functions.h"

#undef DO_API

uint64_t il2cpp_base = 0;

// ------------------------------------------------------------------
// global-metadata.dat header (aka Il2CppGlobalMetadataHeader)
// We only define sanity & version for validation.
// Fields after version vary by Unity version (19~29+), so we do NOT
// rely on a static C struct to calculate the metadata size.
// Instead, we derive size from the memory-mapping region boundary.
// ------------------------------------------------------------------
#pragma pack(push, 1)
struct Il2CppGlobalMetadataHeader {
    int32_t sanity;
    int32_t version;
};
#pragma pack(pop)

static const int32_t kMetadataSanity = 0xFAB11BAF;
static const int32_t kMetadataMinVersion = 16;
static const int32_t kMetadataMaxVersion = 32;
static const size_t  kMaxMetadataSize = 256 * 1024 * 1024; // safety cap: 256 MB

void init_il2cpp_api(void *handle) {
#define DO_API(r, n, p) {                      \
    n = (r (*) p)xdl_sym(handle, #n, nullptr); \
    if(!n) {                                   \
        LOGW("api not found %s", #n);          \
    }                                          \
}

#include "il2cpp-api-functions.h"

#undef DO_API
}

std::string get_method_modifier(uint32_t flags) {
    std::stringstream outPut;
    auto access = flags & METHOD_ATTRIBUTE_MEMBER_ACCESS_MASK;
    switch (access) {
        case METHOD_ATTRIBUTE_PRIVATE:
            outPut << "private ";
            break;
        case METHOD_ATTRIBUTE_PUBLIC:
            outPut << "public ";
            break;
        case METHOD_ATTRIBUTE_FAMILY:
            outPut << "protected ";
            break;
        case METHOD_ATTRIBUTE_ASSEM:
        case METHOD_ATTRIBUTE_FAM_AND_ASSEM:
            outPut << "internal ";
            break;
        case METHOD_ATTRIBUTE_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & METHOD_ATTRIBUTE_STATIC) {
        outPut << "static ";
    }
    if (flags & METHOD_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_FINAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "sealed override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_VIRTUAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_NEW_SLOT) {
            outPut << "virtual ";
        } else {
            outPut << "override ";
        }
    }
    if (flags & METHOD_ATTRIBUTE_PINVOKE_IMPL) {
        outPut << "extern ";
    }
    return outPut.str();
}

bool _il2cpp_type_is_byref(const Il2CppType *type) {
    auto byref = type->byref;
    if (il2cpp_type_is_byref) {
        byref = il2cpp_type_is_byref(type);
    }
    return byref;
}

std::string dump_method(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Methods\n";
    void *iter = nullptr;
    while (auto method = il2cpp_class_get_methods(klass, &iter)) {
        //TODO attribute
        if (method->methodPointer) {
            outPut << "\t// RVA: 0x";
            outPut << std::hex << (uint64_t) method->methodPointer - il2cpp_base;
            outPut << " VA: 0x";
            outPut << std::hex << (uint64_t) method->methodPointer;
        } else {
            outPut << "\t// RVA: 0x VA: 0x0";
        }
        /*if (method->slot != 65535) {
            outPut << " Slot: " << std::dec << method->slot;
        }*/
        outPut << "\n\t";
        uint32_t iflags = 0;
        auto flags = il2cpp_method_get_flags(method, &iflags);
        outPut << get_method_modifier(flags);
        //TODO genericContainerIndex
        auto return_type = il2cpp_method_get_return_type(method);
        if (_il2cpp_type_is_byref(return_type)) {
            outPut << "ref ";
        }
        auto return_class = il2cpp_class_from_type(return_type);
        outPut << il2cpp_class_get_name(return_class) << " " << il2cpp_method_get_name(method)
               << "(";
        auto param_count = il2cpp_method_get_param_count(method);
        for (int i = 0; i < param_count; ++i) {
            auto param = il2cpp_method_get_param(method, i);
            auto attrs = param->attrs;
            if (_il2cpp_type_is_byref(param)) {
                if (attrs & PARAM_ATTRIBUTE_OUT && !(attrs & PARAM_ATTRIBUTE_IN)) {
                    outPut << "out ";
                } else if (attrs & PARAM_ATTRIBUTE_IN && !(attrs & PARAM_ATTRIBUTE_OUT)) {
                    outPut << "in ";
                } else {
                    outPut << "ref ";
                }
            } else {
                if (attrs & PARAM_ATTRIBUTE_IN) {
                    outPut << "[In] ";
                }
                if (attrs & PARAM_ATTRIBUTE_OUT) {
                    outPut << "[Out] ";
                }
            }
            auto parameter_class = il2cpp_class_from_type(param);
            outPut << il2cpp_class_get_name(parameter_class) << " "
                   << il2cpp_method_get_param_name(method, i);
            outPut << ", ";
        }
        if (param_count > 0) {
            outPut.seekp(-2, outPut.cur);
        }
        outPut << ") { }\n";
        //TODO GenericInstMethod
    }
    return outPut.str();
}

std::string dump_property(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Properties\n";
    void *iter = nullptr;
    while (auto prop_const = il2cpp_class_get_properties(klass, &iter)) {
        //TODO attribute
        auto prop = const_cast<PropertyInfo *>(prop_const);
        auto get = il2cpp_property_get_get_method(prop);
        auto set = il2cpp_property_get_set_method(prop);
        auto prop_name = il2cpp_property_get_name(prop);
        outPut << "\t";
        Il2CppClass *prop_class = nullptr;
        uint32_t iflags = 0;
        if (get) {
            outPut << get_method_modifier(il2cpp_method_get_flags(get, &iflags));
            prop_class = il2cpp_class_from_type(il2cpp_method_get_return_type(get));
        } else if (set) {
            outPut << get_method_modifier(il2cpp_method_get_flags(set, &iflags));
            auto param = il2cpp_method_get_param(set, 0);
            prop_class = il2cpp_class_from_type(param);
        }
        if (prop_class) {
            outPut << il2cpp_class_get_name(prop_class) << " " << prop_name << " { ";
            if (get) {
                outPut << "get; ";
            }
            if (set) {
                outPut << "set; ";
            }
            outPut << "}\n";
        } else {
            if (prop_name) {
                outPut << " // unknown property " << prop_name;
            }
        }
    }
    return outPut.str();
}

std::string dump_field(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Fields\n";
    auto is_enum = il2cpp_class_is_enum(klass);
    void *iter = nullptr;
    while (auto field = il2cpp_class_get_fields(klass, &iter)) {
        //TODO attribute
        outPut << "\t";
        auto attrs = il2cpp_field_get_flags(field);
        auto access = attrs & FIELD_ATTRIBUTE_FIELD_ACCESS_MASK;
        switch (access) {
            case FIELD_ATTRIBUTE_PRIVATE:
                outPut << "private ";
                break;
            case FIELD_ATTRIBUTE_PUBLIC:
                outPut << "public ";
                break;
            case FIELD_ATTRIBUTE_FAMILY:
                outPut << "protected ";
                break;
            case FIELD_ATTRIBUTE_ASSEMBLY:
            case FIELD_ATTRIBUTE_FAM_AND_ASSEM:
                outPut << "internal ";
                break;
            case FIELD_ATTRIBUTE_FAM_OR_ASSEM:
                outPut << "protected internal ";
                break;
        }
        if (attrs & FIELD_ATTRIBUTE_LITERAL) {
            outPut << "const ";
        } else {
            if (attrs & FIELD_ATTRIBUTE_STATIC) {
                outPut << "static ";
            }
            if (attrs & FIELD_ATTRIBUTE_INIT_ONLY) {
                outPut << "readonly ";
            }
        }
        auto field_type = il2cpp_field_get_type(field);
        auto field_class = il2cpp_class_from_type(field_type);
        outPut << il2cpp_class_get_name(field_class) << " " << il2cpp_field_get_name(field);
        //TODO 获取构造函数初始化后的字段值
        if (attrs & FIELD_ATTRIBUTE_LITERAL && is_enum) {
            uint64_t val = 0;
            il2cpp_field_static_get_value(field, &val);
            outPut << " = " << std::dec << val;
        }
        outPut << "; // 0x" << std::hex << il2cpp_field_get_offset(field) << "\n";
    }
    return outPut.str();
}

std::string dump_type(const Il2CppType *type) {
    std::stringstream outPut;
    auto *klass = il2cpp_class_from_type(type);
    outPut << "\n// Namespace: " << il2cpp_class_get_namespace(klass) << "\n";
    auto flags = il2cpp_class_get_flags(klass);
    if (flags & TYPE_ATTRIBUTE_SERIALIZABLE) {
        outPut << "[Serializable]\n";
    }
    //TODO attribute
    auto is_valuetype = il2cpp_class_is_valuetype(klass);
    auto is_enum = il2cpp_class_is_enum(klass);
    auto visibility = flags & TYPE_ATTRIBUTE_VISIBILITY_MASK;
    switch (visibility) {
        case TYPE_ATTRIBUTE_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_PUBLIC:
            outPut << "public ";
            break;
        case TYPE_ATTRIBUTE_NOT_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_FAM_AND_ASSEM:
        case TYPE_ATTRIBUTE_NESTED_ASSEMBLY:
            outPut << "internal ";
            break;
        case TYPE_ATTRIBUTE_NESTED_PRIVATE:
            outPut << "private ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAMILY:
            outPut << "protected ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & TYPE_ATTRIBUTE_ABSTRACT && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "static ";
    } else if (!(flags & TYPE_ATTRIBUTE_INTERFACE) && flags & TYPE_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
    } else if (!is_valuetype && !is_enum && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "sealed ";
    }
    if (flags & TYPE_ATTRIBUTE_INTERFACE) {
        outPut << "interface ";
    } else if (is_enum) {
        outPut << "enum ";
    } else if (is_valuetype) {
        outPut << "struct ";
    } else {
        outPut << "class ";
    }
    outPut << il2cpp_class_get_name(klass); //TODO genericContainerIndex
    std::vector<std::string> extends;
    auto parent = il2cpp_class_get_parent(klass);
    if (!is_valuetype && !is_enum && parent) {
        auto parent_type = il2cpp_class_get_type(parent);
        if (parent_type->type != IL2CPP_TYPE_OBJECT) {
            extends.emplace_back(il2cpp_class_get_name(parent));
        }
    }
    void *iter = nullptr;
    while (auto itf = il2cpp_class_get_interfaces(klass, &iter)) {
        extends.emplace_back(il2cpp_class_get_name(itf));
    }
    if (!extends.empty()) {
        outPut << " : " << extends[0];
        for (int i = 1; i < extends.size(); ++i) {
            outPut << ", " << extends[i];
        }
    }
    outPut << "\n{";
    outPut << dump_field(klass);
    outPut << dump_property(klass);
    outPut << dump_method(klass);
    //TODO EventInfo
    outPut << "}\n";
    return outPut.str();
}

void il2cpp_api_init(void *handle) {
    LOGI("il2cpp_handle: %p", handle);
    init_il2cpp_api(handle);
    if (il2cpp_domain_get_assemblies) {
        Dl_info dlInfo;
        if (dladdr((void *) il2cpp_domain_get_assemblies, &dlInfo)) {
            il2cpp_base = reinterpret_cast<uint64_t>(dlInfo.dli_fbase);
        }
        LOGI("il2cpp_base: %" PRIx64"", il2cpp_base);
    } else {
        LOGE("Failed to initialize il2cpp api.");
        return;
    }
    while (!il2cpp_is_vm_thread(nullptr)) {
        LOGI("Waiting for il2cpp_init...");
        sleep(1);
    }
    auto domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);
}

void il2cpp_dump(const char *outDir) {
    LOGI("dumping...");
    size_t size;
    auto domain = il2cpp_domain_get();
    auto assemblies = il2cpp_domain_get_assemblies(domain, &size);
    std::stringstream imageOutput;
    for (int i = 0; i < size; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        imageOutput << "// Image " << i << ": " << il2cpp_image_get_name(image) << "\n";
    }
    std::vector<std::string> outPuts;
    if (il2cpp_image_get_class) {
        LOGI("Version greater than 2018.3");
        //使用il2cpp_image_get_class
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
            imageStr << "\n// Dll : " << il2cpp_image_get_name(image);
            auto classCount = il2cpp_image_get_class_count(image);
            for (int j = 0; j < classCount; ++j) {
                auto klass = il2cpp_image_get_class(image, j);
                auto type = il2cpp_class_get_type(const_cast<Il2CppClass *>(klass));
                //LOGD("type name : %s", il2cpp_type_get_name(type));
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.push_back(outPut);
            }
        }
    } else {
        LOGI("Version less than 2018.3");
        //使用反射
        auto corlib = il2cpp_get_corlib();
        auto assemblyClass = il2cpp_class_from_name(corlib, "System.Reflection", "Assembly");
        auto assemblyLoad = il2cpp_class_get_method_from_name(assemblyClass, "Load", 1);
        auto assemblyGetTypes = il2cpp_class_get_method_from_name(assemblyClass, "GetTypes", 0);
        if (assemblyLoad && assemblyLoad->methodPointer) {
            LOGI("Assembly::Load: %p", assemblyLoad->methodPointer);
        } else {
            LOGI("miss Assembly::Load");
            return;
        }
        if (assemblyGetTypes && assemblyGetTypes->methodPointer) {
            LOGI("Assembly::GetTypes: %p", assemblyGetTypes->methodPointer);
        } else {
            LOGI("miss Assembly::GetTypes");
            return;
        }
        typedef void *(*Assembly_Load_ftn)(void *, Il2CppString *, void *);
        typedef Il2CppArray *(*Assembly_GetTypes_ftn)(void *, void *);
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
            auto image_name = il2cpp_image_get_name(image);
            imageStr << "\n// Dll : " << image_name;
            //LOGD("image name : %s", image->name);
            auto imageName = std::string(image_name);
            auto pos = imageName.rfind('.');
            auto imageNameNoExt = imageName.substr(0, pos);
            auto assemblyFileName = il2cpp_string_new(imageNameNoExt.data());
            auto reflectionAssembly = ((Assembly_Load_ftn) assemblyLoad->methodPointer)(nullptr,
                                                                                        assemblyFileName,
                                                                                        nullptr);
            auto reflectionTypes = ((Assembly_GetTypes_ftn) assemblyGetTypes->methodPointer)(
                    reflectionAssembly, nullptr);
            auto items = reflectionTypes->vector;
            for (int j = 0; j < reflectionTypes->max_length; ++j) {
                auto klass = il2cpp_class_from_system_type((Il2CppReflectionType *) items[j]);
                auto type = il2cpp_class_get_type(klass);
                //LOGD("type name : %s", il2cpp_type_get_name(type));
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.push_back(outPut);
            }
        }
    }
    LOGI("write dump file");
    auto outPath = std::string(outDir).append("/files/dump.cs");
    std::ofstream outStream(outPath);
    outStream << imageOutput.str();
    auto count = outPuts.size();
    for (int i = 0; i < count; ++i) {
        outStream << outPuts[i];
    }
    outStream.close();
    LOGI("dump done!");

    // Generate script.json (IDA symbol table)
    LOGI("generating script.json...");
    std::vector<ScriptMethodEntry> scriptMethods;
    std::set<uint64_t> addressSet;

    for (int i = 0; i < size; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        auto classCount = il2cpp_image_get_class_count(image);
        LOGI("  assembly %d/%d: %d classes", i + 1, size, (int)classCount);
        for (int j = 0; j < classCount; ++j) {
            auto klass = const_cast<Il2CppClass *>(il2cpp_image_get_class(image, j));
            if (!klass) continue;
            void *iter = nullptr;
            while (auto method = il2cpp_class_get_methods(klass, &iter)) {
                if (!method->methodPointer) continue;

                uint64_t rva = (uint64_t) method->methodPointer - il2cpp_base;
                if (rva == 0) continue;

                ScriptMethodEntry entry;
                entry.address = rva;

                // Name: "Namespace.Type$$Method"
                auto ns = il2cpp_class_get_namespace(klass);
                auto cn = il2cpp_class_get_name(klass);
                auto mn = il2cpp_method_get_name(method);
                if (ns && ns[0] != '\0') {
                    entry.name = std::string(ns) + "." + std::string(cn) + "$$" + std::string(mn);
                } else {
                    entry.name = std::string(cn) + "$$" + std::string(mn);
                }

                // Build signature and type signature with error protection
                // to avoid deadlocks from il2cpp_class_from_type on unloaded types
                try {
                    entry.signature = build_signature(method, klass);
                    entry.type_sig = build_type_signature(method);
                } catch (...) {
                    entry.signature = "void _unknown_ ();";
                    entry.type_sig = "v";
                }

                scriptMethods.push_back(entry);
                addressSet.insert(rva);
            }
        }
    }
    LOGI("  collected %zu script methods, %zu addresses", scriptMethods.size(), addressSet.size());

    // Remove null address and sort
    addressSet.erase(0);
    std::vector<uint64_t> addresses(addressSet.begin(), addressSet.end());

    // Dump the (already decrypted) global-metadata.dat from memory.
    // This also finds the metadata pointer so we can extract string literals.
    LOGI("dumping global-metadata.dat...");
    size_t meta_size = 0;
    auto *meta_ptr = il2cpp_dump_global_metadata(outDir, &meta_size);

    // Extract string literals from metadata in memory
    std::vector<StringEntry> scriptStrings;
    if (meta_ptr && meta_size > 0) {
        LOGI("extracting string literals from metadata...");
        scriptStrings = extract_strings(meta_ptr, meta_size, il2cpp_base);
        LOGI("extracted %zu string literals", scriptStrings.size());
    }

    // Determine metadata version for version-dependent behavior
    int32_t metadata_version = 29;  // default to latest
    if (meta_ptr && meta_size >= 8) {
        int32_t ver;
        memcpy(&ver, meta_ptr + 4, 4);
        if (ver >= 16 && ver <= 32) {
            metadata_version = ver;
        }
    }
    set_metadata_version(metadata_version);

    // Extract ScriptMetadata and ScriptMetadataMethod
    std::vector<MetadataEntry> scriptMetadata;
    std::vector<MetadataMethodEntry> scriptMetadataMethod;

    // Try metadataUsage-based extraction first (v19-v24.5)
    if (meta_ptr && meta_size > 0) {
        LOGI("extracting metadata usage from blob...");
        extract_metadata_from_blob(meta_ptr, meta_size, il2cpp_base,
                                   scriptMetadata, scriptMetadataMethod);
    }

    // Always use runtime API as the primary source (provides real Address values)
    LOGI("extracting metadata from runtime API...");
    extract_metadata_from_runtime(scriptMetadata, scriptMetadataMethod);

    // Write script.json with all data (methods + strings + metadata + addresses)
    LOGI("writing script.json (%zu methods, %zu strings, %zu metadata, %zu meta_methods, %zu addresses)...",
         scriptMethods.size(), scriptStrings.size(),
         scriptMetadata.size(), scriptMetadataMethod.size(), addresses.size());
    write_script_json(outDir, scriptMethods, scriptStrings,
                      scriptMetadata, scriptMetadataMethod, addresses);

    // Write stringliteral.json separately
    if (!scriptStrings.empty()) {
        LOGI("writing stringliteral.json...");
        write_stringliteral_json(outDir, scriptStrings);
    }

    // Write il2cpp.h (Phase 4)
    LOGI("generating il2cpp.h...");
    auto type_info = collect_type_info();
    LOGI("collected %zu types for il2cpp.h", type_info.size());
    write_il2cpp_h(outDir, type_info, metadata_version);
    LOGI("all output files generated!");
}

// ------------------------------------------------------------
// Locate global-metadata.dat using the il2cpp API.
//
// The metadata contains all type/method/field definitions and
// string tables.  At runtime, il2cpp API functions like
// il2cpp_image_get_name() return pointers that live inside the
// metadata blob.  We use one such pointer as a "breadcrumb" to
// find the containing memory region, then dump the whole region.
//
// After dumping, we validate the file header (magic + version)
// so we get immediate feedback in logcat whether the output is
// usable by Il2CppDumper.
// ------------------------------------------------------------

static bool validate_metadata_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        LOGE("validate: cannot open %s (errno=%d)", path, errno);
        return false;
    }

    // Read first 8 bytes: magic (uint32 LE) + version (int32 LE)
    uint8_t hdr[8];
    if (fread(hdr, 1, 8, f) != 8) {
        LOGE("validate: file too small (less than 8 bytes)");
        fclose(f);
        return false;
    }

    uint32_t magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) |
                     ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
    int32_t version = (int32_t)hdr[4] | ((int32_t)hdr[5] << 8) |
                      ((int32_t)hdr[6] << 16) | ((int32_t)hdr[7] << 24);

    LOGI("  validation: magic=0x%08X version=%d", magic, version);

    bool magic_ok = (magic == 0xFAB11BAF);
    bool version_ok = (version >= kMetadataMinVersion && version <= kMetadataMaxVersion);

    if (!magic_ok) {
        LOGE("  validation FAILED: magic mismatch (got 0x%08X, expected 0xFAB11BAF)", magic);
    }
    if (!version_ok) {
        LOGE("  validation FAILED: version %d out of range [%d, %d]",
             version, kMetadataMinVersion, kMetadataMaxVersion);
    }

    // Also read header end markers to see how far valid metadata extends
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fclose(f);

    if (magic_ok && version_ok) {
        LOGI("  validation PASSED: magic OK, version=%d, size=%ld bytes (%.2f MB)",
             version, file_size, file_size / (1024.0 * 1024.0));
        LOGI("  file is usable by Il2CppDumper.");
    } else {
        LOGW("  validation WARNING: header may not be parseable by Il2CppDumper");
        LOGI("  file size: %ld bytes", file_size);
    }

    return magic_ok && version_ok;
}

const uint8_t* il2cpp_dump_global_metadata(const char *outDir, size_t *out_meta_size) {
    LOGI("ok dump global-metadata.dat: starting...");

    if (il2cpp_base == 0) {
        LOGE("il2cpp_base is 0, il2cpp API not initialized");
        return nullptr;
    }
    LOGI("il2cpp_base: 0x%" PRIx64, il2cpp_base);

    // --- 1. Get a string pointer that lives inside the metadata ---
    size_t asm_count = 0;
    auto domain = il2cpp_domain_get();
    auto assemblies = il2cpp_domain_get_assemblies(domain, &asm_count);
    if (!assemblies || asm_count == 0) {
        LOGE("no assemblies found - is the game fully loaded?");
        return nullptr;
    }
    LOGI("found %zu assemblies", asm_count);

    for (size_t i = 0; i < asm_count; i++) {
        auto img = il2cpp_assembly_get_image(assemblies[i]);
        LOGD("  assembly[%zu]: %s", i, il2cpp_image_get_name(img));
    }

    auto image = il2cpp_assembly_get_image(assemblies[0]);
    const char *first_name = il2cpp_image_get_name(image);
    if (!first_name) {
        LOGE("first image name is null");
        return nullptr;
    }

    uintptr_t name_ptr = reinterpret_cast<uintptr_t>(first_name);
    LOGI("using image name '%s' at %p as metadata breadcrumb", first_name, (void*)name_ptr);

    // --- 2. Find the /proc/self/maps region containing this pointer ---
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) {
        LOGE("failed to open /proc/self/maps (errno=%d)", errno);
        return nullptr;
    }

    uint64_t region_start = 0, region_end = 0;
    char region_perms[5] = {0};
    char line[512];

    while (fgets(line, sizeof(line), fp)) {
        uint64_t start, end;
        char perms[5] = {0};
        if (sscanf(line, "%" PRIx64 "-%" PRIx64 " %4s %*s %*s %*s %*s",
                   &start, &end, perms) < 3) continue;
        if (name_ptr >= start && name_ptr < end) {
            region_start = start;
            region_end = end;
            memcpy(region_perms, perms, 4);
            region_perms[4] = '\0';
            break;
        }
    }
    fclose(fp);

    if (region_start == 0) {
        LOGE("could not find memory region containing image name at %p", (void*)name_ptr);
        return nullptr;
    }

    size_t meta_size = region_end - region_start;
    if (meta_size > kMaxMetadataSize) {
        LOGW("metadata region %zu MB exceeds cap of %zu MB, clamping",
             meta_size / (1024 * 1024), (size_t)kMaxMetadataSize / (1024 * 1024));
        meta_size = kMaxMetadataSize;
    }

    LOGI("metadata region: [0x%" PRIx64 "-0x%" PRIx64 "] %s, size=%zu (%.2f MB)",
         region_start, region_end, region_perms,
         meta_size, meta_size / (1024.0 * 1024.0));

    // --- 3. Write the region to file ---
    auto outPath = std::string(outDir).append("/files/global-metadata.dat");
    FILE *out = fopen(outPath.c_str(), "wb");
    if (!out) {
        LOGE("failed to create %s (errno=%d)", outPath.c_str(), errno);
        return nullptr;
    }

    const auto *meta_start = reinterpret_cast<const uint8_t *>(region_start);
    size_t written = fwrite(meta_start, 1, meta_size, out);
    fclose(out);

    if (written != meta_size) {
        LOGE("write incomplete: %zu / %zu bytes (errno=%d)", written, meta_size, errno);
        return nullptr;
    }

    LOGI("dumped %zu bytes to %s", written, outPath.c_str());

    // --- 4. Validate the dumped file ---
    LOGI("validating dumped metadata...");
    validate_metadata_file(outPath.c_str());

    LOGI("dump global-metadata.dat: done.");
    if (out_meta_size) *out_meta_size = meta_size;
    return meta_start;
}