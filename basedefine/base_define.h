#ifndef BASE_DEFINE_H
#define BASE_DEFINE_H


// web mode
#define ProactorMode 0
#define ReactorMode 1

// epoll fd trigger mode
#define LevelTriggerMode 0
#define EdgeTriggerMode 1
// total trigger mode
#define List_Conn_LT_LT 0
#define List_Conn_LT_ET 1
#define List_Conn_ET_LT 2
#define List_Conn_ET_ET 3


// log mode
#define LogSyncMode 0
#define LogAsyncMode 1

// log switch
#define LogEnable 1
#define LogDisable 0

// linger switch 
#define LingerEnable 1
#define LingerDisable 0


// timer flag 
#define TimerDisable 0
#define TimerEnable 1

// conn 
#define ReadState 0
#define WriteState 1
#define ConnUnDeal 0
#define ConnIsDeal 1

//

#endif