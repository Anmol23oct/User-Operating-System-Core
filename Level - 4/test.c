#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include "rufs.h"

void just_copy(void* buffer);

void split_path(const char* path){
    int num_levels = 0;
	char path_copy[strlen(path)+1];
    strcpy(path_copy, path);

	while(strcmp(basename(path_copy), "/") != 0){
        strcpy(path_copy, dirname(path_copy));
		num_levels++;
	}
    char *level_names[num_levels+1];
    strcpy(path_copy, path);
    
    for (int i = 0; i < num_levels+1; i++){
        // printf("%s\n", basename(path_copy));
        level_names[i] = basename(path_copy);
        strcpy(path_copy, dirname(path_copy));
    }
    // level_names[2] = basename(path_copy);
    
    for (int i = 0; i < num_levels+1; i++){
        printf("%s\n", level_names[i]);
    }
    printf("%d\n", num_levels);
}

int main() {
    // void* buf;
    // just_copy(buf);
    // // struct dirent* new_ptr = (struct dirent*) buf;
    

    // for (int i = 0; i < 5; i++){
    //     struct dirent* new_ptr = (buf+i*sizeof(struct dirent));
    //     printf("%s\n", new_ptr->name);
    // }
    char* phy_bitmap = malloc(4*sizeof(char));
    for (int i = 0; i < 4; i++){
        phy_bitmap[i] = i;
    }
    void* block_mem = malloc(4096);
    memcpy(block_mem, phy_bitmap, 4*sizeof(char));
    char *dup_bit_map = block_mem;
    for (int i = 0; i < 4; i++){
        printf("%d\n", dup_bit_map[i]);
    }
    
    // split_path(fname);
}

void just_copy(void* buffer){
    struct dirent* new_ent = (struct dirent*) malloc(10 * sizeof(struct dirent));
    strcpy(new_ent->name, "abc");
    new_ent->ino = 4;
    new_ent->valid = 1;
    new_ent->len = 3;

    strcpy((new_ent+1)->name, "def");
    (new_ent+1)->ino = 4;
    (new_ent+1)->valid = 1;
    (new_ent+1)->len = 3;
    for(int i = 0; i < 10; i++){
        printf("Valid: %d\n", (new_ent+i)->valid);
    }

    memcpy( buffer, (void*)new_ent, (10*sizeof(struct dirent)));
}
