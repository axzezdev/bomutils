/*
  dumpbom.cpp - dump internal variables and blocks of bom files (for debugging)

  Copyright (C) 2013 Fabian Renn - fabian.renn (at) gmail.com

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA  02110-1301 USA.

  Initial work done by Joseph Coffland and Julian Devlin.
  Numerous further improvements by Baron Roberts.
*/
#include <iostream>
#include <cstring>
#include <fstream>
#include <string>
#include <iomanip>

#if defined(WINDOWS)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "bom.h"

void print_paths(BOMPaths* paths, char* buffer, BOMBlockTable* block_table, unsigned int id) {
    paths->isLeaf   = ntohs(paths->isLeaf);
    paths->count    = ntohs(paths->count);
    paths->forward  = ntohl(paths->forward);
    paths->backward = ntohl(paths->backward);
    
    std::cout << std::endl;
    std::cout << "path id=" << id << std::endl;
    std::cout << "paths->isLeaf = " << paths->isLeaf << std::endl;
    std::cout << "paths->count = " << paths->count << std::endl;
    std::cout << "paths->forward = " << paths->forward << std::endl;
    std::cout << "paths->backward = " << paths->backward << std::endl;
    
    for (unsigned int i = 0; i < paths->count; ++i) {
        BOMPointer& ptr  = block_table->blockPointers[ntohl(paths->indices[i].index1)];
        BOMFile&    file = *((BOMFile*)&buffer[ptr.address]);
        std::cout << "path->indices[" << i << "].index0 = " << ntohl(paths->indices[i].index0) << std::endl;
        std::cout << "path->indices[" << i << "].index1.parent = " << ntohl(file.parent) << std::endl;
        std::cout << "path->indices[" << i << "].index1.name = " << file.name << std::endl;
    }
    
    if (paths->isLeaf == htons(0)) {
        BOMPointer& child_ptr   = block_table->blockPointers[ntohl(paths->indices[0].index0)];
        BOMPaths*   child_paths = (BOMPaths*)&buffer[child_ptr.address];
        print_paths(child_paths, buffer, block_table, ntohl(paths->indices[0].index0));
    }
    
    if (paths->forward) {
        BOMPointer& sibling_ptr   = block_table->blockPointers[paths->forward];
        BOMPaths*   sibling_paths = (BOMPaths*)&buffer[sibling_ptr.address];
        print_paths(sibling_paths, buffer, block_table, paths->forward);
    }
}

void print_tree(BOMTree* tree, char* buffer, BOMBlockTable* block_table) {
    tree->version   = ntohl(tree->version);
    tree->child     = ntohl(tree->child);
    tree->blockSize = ntohl(tree->blockSize);
    tree->pathCount = ntohl(tree->pathCount);
    std::string type(tree->tree, 4);
    
    std::cout << "tree->tree = " << type << std::endl;
    std::cout << "tree->version = " << tree->version << std::endl;
    std::cout << "tree->child = " << tree->child << std::endl;
    std::cout << "tree->blockSize = " << tree->blockSize << std::endl;
    std::cout << "tree->pathCount = " << tree->pathCount << std::endl;
    std::cout << "tree->unknown3 = " << (int)tree->unknown3 << std::endl;
    BOMPointer& child_ptr = block_table->blockPointers[tree->child];
    BOMPaths*   paths     = (BOMPaths*)&buffer[child_ptr.address];
    print_paths(paths, buffer, block_table, tree->child);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: dumpbom bomfile" << std::endl;
        return 1;
    }
    
    char*     buffer;
    std::streampos file_length;
    {
        std::ifstream bom_file(argv[1], std::ios::binary | std::ios::in);
        
        bom_file.seekg(0, std::ios::end);
        file_length = bom_file.tellg();
        bom_file.seekg(0);
        
        buffer = new char[file_length];
        bom_file.read(buffer, file_length);
        
        if (bom_file.fail()) {
            std::cerr << "Unable to read bomfile" << std::endl;
            return 1;
        }
        
        bom_file.close();
    }
    
    std::cout << argv[1] << std::endl;
    std::cout << "file_length = " << file_length << std::endl;
    
    std::cout << "Header:" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    
    BOMHeader* header      = (BOMHeader*)buffer;
    header->version        = ntohl(header->version);
    header->numberOfBlocks = ntohl(header->numberOfBlocks);
    header->indexOffset    = ntohl(header->indexOffset);
    header->indexLength    = ntohl(header->indexLength);
    header->varsOffset     = ntohl(header->varsOffset);
    header->varsLength     = ntohl(header->varsLength);
    
    BOMBlockTable* block_table              = (BOMBlockTable*)&buffer[header->indexOffset];
    block_table->numberOfBlockTablePointers = ntohl(block_table->numberOfBlockTablePointers);
    int numberOfNonNullEntries              = 0;
    
    for (unsigned int i = 0; i < block_table->numberOfBlockTablePointers; ++i) {
        if (block_table->blockPointers[i].address != 0) {
            numberOfNonNullEntries++;
            block_table->blockPointers[i].address = ntohl(block_table->blockPointers[i].address);
            block_table->blockPointers[i].length  = ntohl(block_table->blockPointers[i].length);
        }
    }
    
    {
        std::string magic(header->magic, 8);
        std::cout << "magic = \"" << magic << "\"" << std::endl;
        std::cout << "version = " << header->version << std::endl;
        std::cout << "numberOfBlocks = " << header->numberOfBlocks << std::endl;
        std::cout << "indexOffset = " << header->indexOffset << std::endl;
        std::cout << "indexLength = " << header->indexLength << std::endl;
        std::cout << "varsOffset = " << header->varsOffset << std::endl;
        std::cout << "varsLength = " << header->varsLength << std::endl;
        std::cout << "(calculated number of blocks = " << numberOfNonNullEntries << ")" << std::endl;
    }
    
    std::cout << std::endl << "Index Table:" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    
    std::cout << "numberOfBlockTableEntries = " << block_table->numberOfBlockTablePointers << std::endl;
#if 0
  for ( unsigned int i=0; i < block_table->numberOfBlockTablePointers; ++i ) {
    if ( block_table->blockPointers[i].address != 0 ) {
      std::cout << "{" << std::endl;
      std::cout << "\tid = " << i << std::endl;
      std::cout << "\taddress = " << std::setbase(16) << "0x" << block_table->blockPointers[i].address << std::setbase(10) << std::endl;
      std::cout << "\tlength = " << block_table->blockPointers[i].length << std::endl;
      std::cout << "}," << std::endl;
    }
  }
#endif
    
    uint32_t free_list_pos = header->indexOffset + sizeof(uint32_t) +
                             (block_table->numberOfBlockTablePointers * sizeof(BOMPointer));
    BOMFreeList* free_list              = (BOMFreeList*)&buffer[free_list_pos];
    free_list->numberOfFreeListPointers = ntohl(free_list->numberOfFreeListPointers);
    std::cout << std::endl << "Free List:" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "numberOfFreeListPointers = " << free_list->numberOfFreeListPointers << std::endl;
    for (unsigned int i = 0; i < free_list->numberOfFreeListPointers; ++i) {
        free_list->freelistPointers[i].address = ntohl(free_list->freelistPointers[i].address);
        free_list->freelistPointers[i].length  = ntohl(free_list->freelistPointers[i].length);
    }
#if 0
  for ( unsigned int i=0; i < free_list->numberOfFreeListPointers; ++i ) {
    std::cout << "{" << std::endl;
    std::cout << "\tid = " << i << std::endl;
    std::cout << "\taddress = " << std::setbase(16) << "0x" << free_list->freelistPointers[i].address << std::setbase(10) << std::endl;
    std::cout << "\tlength = " << free_list->freelistPointers[i].length << std::endl;
    std::cout << "}," << std::endl;
  }
#endif
    
    std::cout << std::endl << "Variables:" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;
    
    int var_count;
    {
        BOMVars*     vars         = (BOMVars*)&buffer[header->varsOffset];
        unsigned int total_length = 0;
        vars->count               = ntohl(vars->count);
        total_length += sizeof(uint32_t);
        BOMVar* var = &vars->first[0];
        var_count   = vars->count;
        for (int i = 0; i < var_count; ++i) {
            var->index = ntohl(var->index);
            total_length += sizeof(uint32_t);
            total_length += var->length + 1;
            var = (BOMVar*)&buffer[header->varsOffset + total_length];
        }
        
        std::cout << "vars->count = " << vars->count << std::endl;
        std::cout << "( calculated length = " << total_length << ")" << std::endl;
        var          = &vars->first[0];
        total_length = sizeof(uint32_t);
        
        for (int i = 0; i < var_count; ++i) {
            if (i != 0) {
                std::cout << ",";
            }
            total_length += sizeof(uint32_t);
            total_length += var->length + 1;
            std::string name(var->name, var->length);
            std::cout << "\"" << name << "\"";
            var = (BOMVar*)&buffer[header->varsOffset + total_length];
        }
        std::cout << std::endl;
    }

    unsigned int total_length = sizeof(uint32_t);
    for (int i = 0; i < var_count; ++i) {
        BOMVar* var = (BOMVar*)&buffer[header->varsOffset + total_length];
        total_length += sizeof(uint32_t) + 1 + var->length;
        std::string      name(var->name, var->length);
        BOMPointer& ptr = block_table->blockPointers[var->index];
        std::cout << std::endl
                  << "\"" << name << "\" (file offset: 0x" << std::setbase(16) << ptr.address << std::setbase(10)
                  << " length: " << ptr.length << " )" << std::endl;
        std::cout << "-----------------------------------------------------" << std::endl;
        if ((name == "Paths") || (name == "HLIndex") || (name == "Size64")) {
            BOMTree* tree = (BOMTree*)&buffer[ptr.address];
            print_tree(tree, buffer, block_table);
        } else if (name == "BomInfo") {
            BOMInfo* info             = (BOMInfo*)&buffer[ptr.address];
            info->version             = ntohl(info->version);
            info->numberOfPaths       = ntohl(info->numberOfPaths);
            info->numberOfInfoEntries = ntohl(info->numberOfInfoEntries);
            for (unsigned int i = 0; i < info->numberOfInfoEntries; ++i) {
                info->entries[i].unknown0 = ntohl(info->entries[i].unknown0);
                info->entries[i].unknown1 = ntohl(info->entries[i].unknown1);
                info->entries[i].unknown2 = ntohl(info->entries[i].unknown2);
                info->entries[i].unknown3 = ntohl(info->entries[i].unknown3);
            }
            std::cout << "info->version = " << info->version << std::endl;
            std::cout << "info->numberOfPaths = " << info->numberOfPaths << std::endl;
            std::cout << "info->numberOfInfoEntries = " << info->numberOfInfoEntries << std::endl;
            for (unsigned int i = 0; i < info->numberOfInfoEntries; ++i) {
                std::cout << "info->entries[" << i << "].unknown0 = " << info->entries[i].unknown0
                          << std::endl;
                std::cout << "info->entries[" << i << "].unknown1 = " << info->entries[i].unknown1
                          << std::endl;
                std::cout << "info->entries[" << i << "].unknown2 = " << info->entries[i].unknown2
                          << std::endl;
                std::cout << "info->entries[" << i << "].unknown3 = " << info->entries[i].unknown3
                          << std::endl;
            }
        } else if (name == "VIndex") {
            BOMVIndex* vindex    = (BOMVIndex*)&buffer[ptr.address];
            vindex->unknown0     = ntohl(vindex->unknown0);
            vindex->indexToVTree = ntohl(vindex->indexToVTree);
            vindex->unknown2     = ntohl(vindex->unknown2);
            // vindex->unknown3 is a byte so conversion not needed
            std::cout << "vindex->unknown0 = " << vindex->unknown0 << std::endl;
            std::cout << "vindex->indexToVTree = " << vindex->indexToVTree << std::endl;
            std::cout << "vindex->unknown2 = " << vindex->unknown2 << std::endl;
            std::cout << "vindex->unknown3 = " << (int)vindex->unknown3 << std::endl;
            std::cout << std::endl;
            BOMPointer& v_ptr = block_table->blockPointers[vindex->indexToVTree];
            BOMTree*    tree  = (BOMTree*)&buffer[v_ptr.address];
            print_tree(tree, buffer, block_table);
        } else {
            unsigned int i;
            uint32_t*    raw = (uint32_t*)&buffer[ptr.address];
            for (i = 0; i < ptr.length / sizeof(uint32_t); ++i) {
                std::cout << "0x" << std::setbase(16) << std::setw(8) << std::setfill('0') << ntohl(raw[i])
                          << std::setbase(10) << std::endl;
            }
            i *= sizeof(uint32_t);
            for (; i < ptr.length; ++i) {
                std::cout << "0x" << std::setbase(16) << std::setw(2) << std::setfill('0')
                          << (int)buffer[ptr.address + i] << std::endl;
            }
        }
    }

    delete[] buffer;
    return 0;
}