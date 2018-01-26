#ifndef IONVIDEO_EXT_H
#define IONVIDEO_EXT_H

extern int ionvideo_assign_map(char **receiver_name, int *inst);

extern int ionvideo_alloc_map(char **receiver_name, int *inst);

extern void ionvideo_release_map(int inst);

#endif /* IONVIDEO_EXT_H */
