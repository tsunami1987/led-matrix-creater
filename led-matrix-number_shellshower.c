#include <stdio.h>    // For printf
#include <stdlib.h>   // For system("clear")
#include <string.h>   // For memset
#include <unistd.h>   // For usleep (only for Linux/Unix simulation)


/*
本算法将两个8x8的led扩展为16x16的矩阵，以便将两个8x8的led矩阵完全嵌入到16x16的矩阵中，
从而形成一个斜着放的沙漏在16x16的矩阵中，从game_grid变量中可以看出一个斜着放的沙漏
方便整体渲染，但耗费内存较多
在整体渲染的过程中，中间两个8x8led的连接处可以直接计算，两个8x8矩阵也可以整体计算不用分离处理
本算法将重力方向分为8种，降低重力的计算量
将像素led的遍历方式分为4种，对每一种方式，预先设计好下落方向

*/

// 定义点阵的尺寸
#define GRID_SIZE 8  //只能是8，大于8需要将game_gridup 改为unsigned short

// 对象类型，使用2bit表示
#define OBJ_TYPE_EMPTY 0 // 0
#define OBJ_TYPE_BALL 1  // 1


#define MAKE_BYTE(b7, b6, b5, b4, b3, b2, b1, b0) \
    ((b7 << 7) | (b6 << 6) | (b5 << 5) | (b4 << 4) | (b3 << 3) | (b2 << 2) | (b1 << 1) | (b0 << 0))

#define GET_BYTE_BIT(byted, index) (byted & (0x80 >> index))
#define SET_BYTE_BIT(byted, index) (byted |= (0x80 >> index))
#define CLS_BYTE_BIT(byted, index) (byted &= ~(0x80 >> index))



// 全局网格，存储每个位置的对象状态
unsigned char game_gridup[GRID_SIZE]={
MAKE_BYTE(0, 0, 0, 1, 1, 0, 0, 1),
MAKE_BYTE(0, 0, 0, 0, 1, 0, 1, 0),
MAKE_BYTE(0, 0, 1, 0, 1, 0, 1, 0),
MAKE_BYTE(0, 0, 1, 1, 0, 0, 0, 0),
MAKE_BYTE(0, 0, 0, 0, 0, 0, 0, 0),
MAKE_BYTE(0, 0, 0, 0, 0, 0, 0, 0),
MAKE_BYTE(0, 0, 0, 0, 0, 0, 0, 0),
MAKE_BYTE(0, 0, 0, 0, 0, 0, 0, 0),
};

unsigned char game_griddown[GRID_SIZE]={
MAKE_BYTE(0, 0, 0, 0, 0, 0, 0, 0),
MAKE_BYTE(0, 0, 0, 0, 0, 0, 0, 0),
MAKE_BYTE(0, 0, 0, 0, 0, 0, 0, 0),
MAKE_BYTE(0, 0, 0, 0, 0, 0, 0, 0),
MAKE_BYTE(0, 0, 0, 0, 0, 0, 0, 0),
MAKE_BYTE(0, 0, 0, 0, 0, 0, 0, 0),
MAKE_BYTE(0, 0, 0, 0, 0, 0, 0, 0),
MAKE_BYTE(0, 0, 0, 0, 0, 0, 0, 0),
};

// ===========================================
// === 辅助函数实现 ===
// ===========================================

/**
 * @brief 一个简单的伪随机数生成器，适用于C51。
 * @return unsigned char 返回一个0-255的伪随机数。
 */
unsigned char rand_simple(void) {
    static unsigned char seed = 1; // 使用静态变量作为种子
    seed = (seed * 11 + 7) % 256;
    return seed;
}

/**
 * @brief 在shell中输出图像模拟，以1表示墙壁，2表示球，0表示空位。
 */
void print_grid_to_shell(char isup, unsigned char game_grid[GRID_SIZE]) {
    
    unsigned char r, c;
    for (r = 0; r < GRID_SIZE; r++) {
        if(isup)printf("                ");
        for (c = 0; c < GRID_SIZE; c++) {
            if (GET_BYTE_BIT(game_grid[r], c)) {
                printf("1 ");
            } else {
                printf("0 ");
            }
        }
        printf("\n");
    }
}

// 3x5 数字字模 (每个字节的低3位有效)
const uint8_t FONT_3x5[] = {
    0x07, 0x05, 0x05, 0x05, 0x07, // 0: 111, 101, 101, 101, 111
    0x02, 0x02, 0x02, 0x02, 0x02, // 1: 010, 010, 010, 010, 010
    0x07, 0x01, 0x07, 0x04, 0x07, // 2
    0x07, 0x01, 0x07, 0x01, 0x07, // 3
    0x05, 0x05, 0x07, 0x01, 0x01, // 4
    0x07, 0x04, 0x07, 0x01, 0x07, // 5
    0x07, 0x04, 0x07, 0x05, 0x07, // 6
    0x07, 0x01, 0x01, 0x01, 0x01, // 7
    0x07, 0x05, 0x07, 0x05, 0x07, // 8
    0x07, 0x05, 0x07, 0x01, 0x07  // 9
};

void get_frame_0_to_99(unsigned char *output_frame, unsigned char number) {
    unsigned char tens = number / 10;
    unsigned char units = number % 10;

    // 清空背景（第 0, 6, 7 行通常留空，因为数字只有 5 行高）
    for(unsigned char i = 0; i < 8; i++) output_frame[i] = 0x00;

    // 填充中间的 5 行 (第 1 到第 5 行)
    for (unsigned char i = 0; i < 5; i++) {
        uint8_t left_part = FONT_3x5[tens * 5 + i] << 4;  // 十位移到左侧
        uint8_t right_part = FONT_3x5[units * 5 + i];    // 个位在右侧
        output_frame[i + 1] = left_part | right_part;   // 合并
    }
}

/*屏幕旋转90度显示*/
void generate_digit_frame_turn90(unsigned char *screen_buffer, unsigned char number) {
    unsigned char tens = number / 10;
    unsigned char units = number % 10;
    
    // 初始化清空画布
    for(unsigned char i = 0; i < 8; i++) screen_buffer[i] = 0;

    for (unsigned char row = 0; row < 5; row++) {
        // 获取字模的一行
        uint8_t row_data_tens = FONT_3x5[tens * 5 + row];
        uint8_t row_data_units = FONT_3x5[units * 5 + row];

        // 拼接成 8 位：十位占高 4 位，个位占低 4 位
        // 结果格式：[T T T 0 U U U 0] (T=十位, U=个位)
        uint8_t combined_row = (row_data_tens << 5) | (row_data_units << 1);
#if 0
        // 应用旋转 90 度映射到 screen_buffer
        for (unsigned char col = 0; col < 8; col++) {
            if ((combined_row >> (7 - col)) & 0x01) {
                // 原本 (col, row) 为亮，旋转 90 度后映射
                // 顺时针 90: new_row = col, new_col = 7 - row
                screen_buffer[col] |= (1 << (7 - (7 - row))); 
                // 简写：screen_buffer[col] |= (1 << row);
            }
        }
#endif
    }
}

void shownumber(char num){
		get_frame_0_to_99(game_gridup, num);
}

// ===========================================
// === 主程序 (用于Linux模拟，可移植到C51) ===
// ===========================================

int main() {
    char num = 0;
 
    while (1) {
    	  if(num >= 100)num = 0;
				shownumber(num++);
        usleep(1000000); // 延时200毫秒，控制模拟速度
    }

    printf("\n所有球已停止移动。\n");
    return 0;
}
