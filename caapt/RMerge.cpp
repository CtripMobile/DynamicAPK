//
//  RMerge.c
//  RMerge
//
//  Created by jim on 15/4/9.
//  Copyright (c) 2015年 ctrip. All rights reserved.
//

#include "RMerge.h"

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define k_flag_template "public static final class %s {"
#define k_package_flag "package "
#define k_start_class_flag "public static final class "
#define k_end_class_flag "}"

unsigned long get_file_size(const char *path) {
    if (path == NULL) {
        return 0;
    }
    
    unsigned long filesize = -1;
    struct stat statbuff;
    if(stat(path, &statbuff) < 0){
        return filesize;
    }else{
        filesize = statbuff.st_size;
    }
    return filesize;
}

char *copy_start_flag_with_tag(const char *tag) {
    if (tag == NULL) {
        return NULL;
    }
    
    const char *flag_template = k_flag_template;
    long flag_len = strlen(flag_template) + strlen(tag) + 4;
    char *start_flag  = (char *)malloc(flag_len);
    memset(start_flag, '\0', flag_len);
    sprintf(start_flag, flag_template, tag);
    return start_flag;
}

char *copy_java_with_flag(const char *in_str, const char *start_anim_flag, const char *end_anim_flag) {
    if (in_str == NULL || start_anim_flag == NULL || end_anim_flag == NULL) {
        return NULL;
    }
    
    char *find_str  = strstr(in_str, start_anim_flag);
    char *out_str = NULL;
    
    
    if (find_str != NULL) {
        long out_len = strlen(in_str)*sizeof(in_str) + 10;
        out_str = (char *)malloc(out_len);
        memset(out_str, '\0', out_len);
        find_str += strlen(start_anim_flag);
        
        int k = 0;
        char ch = '\0';
        int find_tag_times = 1;
        while ((ch=*(find_str++))) {
            
            if ( ch == '{') {
                find_tag_times++;
            }
            else if (ch == '}') {
                find_tag_times--;
            }
            
            if (find_tag_times <= 0 && ch == '}') {
                break;
            }
            else {
                out_str[k++] = ch;
            }
        }
    }
    return out_str;
}

void c_free(void *pointer) {
    if (pointer != NULL) {
        free(pointer);
    }
}

int merge_r_file(const char* public_r_file_path, const char* project_r_file_path) {
    if (public_r_file_path == NULL || project_r_file_path == NULL) {
        printf("***********Error, in param in null, return -1;\n");
        return -1;
    }
    
    FILE *public_r_fp = fopen(public_r_file_path, "r");
    FILE *project_r_fp = fopen(project_r_file_path, "r");
    
    if (public_r_fp == NULL || project_r_fp == NULL) {
        printf("***********Error, read R.java failed, return -2;\n");
        return -2;
    }
    
    unsigned long public_r_file_size = get_file_size(public_r_file_path);
    unsigned long project_r_file_size = get_file_size(project_r_file_path);
    
    unsigned long public_r_malloc_len = (unsigned long)(public_r_file_size + 1);
    unsigned long project_r_malloc_len = (unsigned long)(project_r_file_size + 1);
    unsigned long output_r_malloc_len = (unsigned long)(public_r_file_size + project_r_file_size+2);
    
    char *public_r_str = (char *)malloc(public_r_malloc_len);
    char *public_r_formated_str = (char *)malloc(public_r_malloc_len);
    char *project_r_str = (char *)malloc(project_r_malloc_len);
    char *output_r_str = (char *)malloc(output_r_malloc_len);
    
    memset(public_r_str, '\0', public_r_malloc_len);
    memset(public_r_formated_str, '\0', public_r_malloc_len);
    memset(project_r_str, '\0', project_r_malloc_len);
    memset(output_r_str, '\0', output_r_malloc_len);
    
    size_t public_r_read_len = fread(public_r_str, 1 , public_r_file_size, public_r_fp);
    size_t project_r_read_len = fread(project_r_str, 1, project_r_file_size, project_r_fp);
    
    if (public_r_read_len == public_r_file_size && project_r_read_len == project_r_file_size) {
        printf("***********[%s]、[%s],文件读取成功\n", public_r_file_path, project_r_file_path);
        
        char *find_package = strstr(project_r_str, k_package_flag);
        if (find_package != NULL) {
            char package_path[128] = {0};
            
            char pch = '\0';
            int m = 0;
            while ((pch = (*find_package++)) != '\n') {
                package_path[m++] = pch;
            }
            printf("***********package name==%s\n", package_path);
            
            strcpy(output_r_str, package_path);
        }
        
        strcat(output_r_str, "\npublic final class R {\n");
        
        int is_flag = 0;
        int j = 0;
        int i = 0;
        for (; i < public_r_read_len; i++) {
            if (i >= 0 && i <= public_r_read_len-4) {
                if (public_r_str[i]== '/' && public_r_str[i+1] == '*' ) { //&& r_str[i+2] == '*'
                    is_flag = 1;
                }
                else if (public_r_str[i-2] == '*' && public_r_str[i-1] == '/') {
                    is_flag = 0;
                }
                
                if (is_flag != 1) {
                    public_r_formated_str[j++] = public_r_str[i];
                }
            }
        }
        
        public_r_formated_str[j++] = '\0';
        printf("***********Public R文件格式化完成!\n");
        const char *end_anim_flag = k_end_class_flag;
        
        const char *s_tag =  k_start_class_flag;
        char *find_str = strstr(public_r_formated_str,s_tag);
        
        while (find_str) {
            find_str+= strlen(s_tag);
            char tag_d[64]= {0};
            int k = 0;
            char ch = '\0';
            while ((ch=*(find_str++)) != ' ') {
                tag_d[k++] = ch;
            }
            
            if (ch == ' ') {
                printf("***********开始Merge tag:%s\n", tag_d);
                char *start_flag = copy_start_flag_with_tag(tag_d);
                char *array = copy_java_with_flag(public_r_formated_str, start_flag, end_anim_flag);
                char *array2 = copy_java_with_flag(project_r_str, start_flag, end_anim_flag);
                find_str = strstr(find_str,s_tag);
                strcat(output_r_str, "\n");
                strcat(output_r_str, start_flag);
                
                if (array != NULL) {
                    strcat(output_r_str, array);
                }
                if (array2 != NULL) {
                    strcat(output_r_str, array2);
                }
                strcat(output_r_str, "    ");
                strcat(output_r_str, end_anim_flag);
                
                c_free(start_flag);
                c_free(array);
                c_free(array2);
            }
        }
        
        strcat(output_r_str, "\n}");
    }
    else {
        printf("***********R.java file读取长度有错误, return -3;\n");
        return -3;
    }
    
    
    fclose(project_r_fp);
    fclose(public_r_fp);
    
    //    printf("ret_str===%s\n", output_r_str);
    
//    project_r_file_path = "tmp_R_d.java";
    FILE *out_project_fp = fopen(project_r_file_path, "w");
    if (out_project_fp != NULL) {
        size_t write_ret = fwrite(output_r_str, strlen(output_r_str), 1, out_project_fp);
        fclose(out_project_fp);
        
        if (write_ret == 1) {
            printf("***********写R文件成功成功!Path=[%s]\n", project_r_file_path);
        } else {
            printf("***********写文件失败, write_ret==%ld\n", write_ret);
        }
    }
    
    c_free(public_r_str);
    c_free(public_r_formated_str);
    c_free(project_r_str);
    c_free(output_r_str);
    
    return 1;
}

