//
//  RMerge.h
//  RMerge
//
//  Created by jim on 15/4/9.
//  Copyright (c) 2015年 ctrip. All rights reserved.
//

#ifndef __RMerge__RMerge__
#define __RMerge__RMerge__

#include <stdio.h>

/**
 * Merge 子工程R文件和公共工程R文件，merge之后，会自动修改子工程R文件
 * @param project_r_file_path 子工程R文件路径
 * @param public_r_file_path  公共工程R文件路径
 * @return 成功返回1
 */
int merge_r_file(const char* public_r_file_path, const char* project_r_file_path);

#endif /* defined(__RMerge__RMerge__) */
