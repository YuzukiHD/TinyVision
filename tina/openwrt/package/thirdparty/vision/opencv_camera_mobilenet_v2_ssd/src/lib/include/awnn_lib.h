#ifndef __AWNN_LIB_H__
#define __AWNN_LIB_H__
#ifdef __cplusplus
       extern "C" {
#endif

typedef struct Awnn_Context Awnn_Context_t;

// 初始化，每个应用进程只需调用1次，输入参数为预留heap内存大小
void awnn_init(unsigned int mem_size);
// 反初始化，应用进程退出时或不使用NPU时调用
void awnn_uninit();
// 通过nbg文件创建网络模型，返回Awnn_Context_t指针
Awnn_Context_t *awnn_create(char *nbg);
// 销毁Awnn_Context_t指定的网络
void awnn_destroy(Awnn_Context_t *context);
// 设置输入buffer
void awnn_set_input_buffers(Awnn_Context_t *context, unsigned char **input_buffers);
// 获取输出buffer
float **awnn_get_output_buffers(Awnn_Context_t *context);
// 运行Awnn_Context_t指定网络
void awnn_run(Awnn_Context_t *context);
// dump出网络的tensor
void awnn_dump_io(Awnn_Context_t *context, const char *path);

#ifdef __cplusplus
}
#endif

#endif
