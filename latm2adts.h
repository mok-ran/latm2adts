//
//  latm2adts.h
//  LATM2ADTS
//
//  Created by mini on 2017/5/19.
//  Copyright © 2017年 mini. All rights reserved.
//

#ifndef latm2adts_h
#define latm2adts_h
#include <stdio.h>
#include <stdint.h>

enum{
    L2AERROR_INVAILD_PARA = -1,
    L2AERROR_INVAILD_DATA = -2,
    L2AERROR_PATCHWELCOME = -3,
    L2AERROR_ENOMEM       = -12,/* Cannot allocate memory */
    L2AERROR_ENOSYS       = -78,/* Function not implemented */
    L2A_SUCCESS           =  0,
};
//muxConfigPresent
typedef struct {//带外传的话，需要AudioSpecificConfig信息,否则传NULL
    int audio_object_type;
    int sampling_frequency;
    int channel_configuration;
}AudioSpecificConfig;

/**
 * uint8_t *adts_data = NULL;
 * int adts_size = 0;
 * int ret = 0;
 * if((ret = func_latm2adts(packet->data, packet->size, &adts_data, &adts_size, NULL)) == L2A_SUCCESS){
 *   do something
 *   free(adts_frame_data)
 * }else{
 *   printf("func_latm2adts error %d\n",ret);
 *   check error
 * }
 *  free(latm_frame_data)
 *  latm_frame_data , adts_frame_data(if success) free by user
 *  @return 0 if OK, < 0 on error
 */
int func_latm2adts(uint8_t *latm_frame_data,int latm_frame_size,uint8_t **adts_frame_data,int *adts_frame_size,AudioSpecificConfig *asc);



#endif /* latm2adts_h */
