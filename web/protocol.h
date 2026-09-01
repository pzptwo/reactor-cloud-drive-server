#ifndef PROTOCOL_H
#define PROTOCOL_H

//设计自定义的通讯协议
//由于长度都是大于0的
//#define unsigned int uint
typedef unsigned int uint;  //两个的阶段不同（编译与预处理）
#define REGISTER_OK "register_ok"
#define REGISTER_FALSE  "register_false:name existed"

#define LOGIN_OK "login_ok"
#define LOGIN_FALSE  "login_false:name error or pwd error or relogin"

#define SEARCH_ONLINE "existd and online"
#define SEARCH_OFFLINE "existed but offline"
#define SEARCH_NOPERSON "not exist"

#define UNKNOWN_ERROR "unknown error"
#define ADD_FRIEND_OFFLINE "user offline"
#define EXITED_FRIEND "user had been your friend"
#define NOT_EXISTED "user not existed"
#define SEND_ADD_FRIEND "send add friend"

#define DEL_FRIEND_OK "delete friend ok"

#define DIR_NOT_EXISTED "dir not exist"
#define FILE_EXIST "file exist"
#define FILE_CREATE_OK "file create ok"

#define DEL_DIR_OK "del ok"
#define DEL_DIR_FLASE "del_dir_false :is file"

#define RENAME_OK "rename ok"
#define RENAME_FLASE "rename false"
#define ENTRY_DIR_FLASE "entry_dir_false"

#define UPDATE_OK "update ok"
#define UPDATE_FALSE "update false"

#define DEL_FILE_OK "del ok"
#define DEL_FILE_FLASE "del_file_false :is dir"

#define MOVE_DIR_OK "move ok"
#define MOVE_DIR_FLASE "move_dir_false :path is filepath"

#define COMMEN_ERR "operate fail: system is busy"
//需要分通信协议的类型了
enum ENUM_MSG_TYPE
{
    ENUM_MSG_TYPE_MIN=0,
    ENUM_MSG_TYPE_REGISTER_RESPEST, //注册逻辑
    ENUM_MSG_TYPE_REGISTER_RESPONSE,

    ENUM_MSG_TYPE_LOGIN_RESPEST, //登录逻辑
    ENUM_MSG_TYPE_LOGIN_RESPONSE,

    ENUM_MSG_TYPE_ALL_ONLINE_RESPEST, //所有在线
    ENUM_MSG_TYPE_ALL_ONLINE_RESPONSE,

    ENUM_MSG_TYPE_SEARCH_USER_RESPEST, //搜索用户
    ENUM_MSG_TYPE_SEARCH_USER_RESPONSE,

    ENUM_MSG_TYPE_ADD_USER_RESPEST, //加用户，（pdu里面先传addUser,后是登录的名字）
    ENUM_MSG_TYPE_ADD_USER_RESPONSE,

    ENUM_MSG_TYPE_ADD_USER_AGREED,
    ENUM_MSG_TYPE_ADD_USER_REFUSE,

    ENUM_MSG_TYPE_FLUSH_FRIEND_RESPEST, //刷新好友列表
    ENUM_MSG_TYPE_FLUSH_FRIEND_RESPONSE,

    ENUM_MSG_TYPE_DEL_FRIEND_RESPEST, //删除好友
    ENUM_MSG_TYPE_DEL_FRIEND_RESPONSE,

    ENUM_MSG_TYPE_PRIVATE_CHAT_RESPEST, //私聊
    ENUM_MSG_TYPE_PRIVATE_CHAT_RESPONSE,

    ENUM_MSG_TYPE_GROUP_CHAT_RESPEST, //群发，与在线有关
    ENUM_MSG_TYPE_GROUP_CHAT_RESPONSE,

    ENUM_MSG_TYPE_CREATE_DIR_RESPEST, //创建文件夹
    ENUM_MSG_TYPE_CREATE_DIR_RESPONSE,

    ENUM_MSG_TYPE_FLUSH_DIR_RESPEST, //刷新文件夹,获得实时的文件夹信息
    ENUM_MSG_TYPE_FLUSH_DIR_RESPONSE,

    ENUM_MSG_TYPE_DEL_DIR_RESPEST, //删除文件夹
    ENUM_MSG_TYPE_DEL_DIR_RESPONSE,

    ENUM_MSG_TYPE_RENAME_FILE_RESPEST, //重命名文件
    ENUM_MSG_TYPE_RENAME_FILE_RESPONSE,

    ENUM_MSG_TYPE_ENTRY_DIR_RESPEST, //进入文件夹
    ENUM_MSG_TYPE_ENTRY_DIR_RESPONSE,

    ENUM_MSG_TYPE_UPDATE_FILE_RESPEST, //上传文件。
    ENUM_MSG_TYPE_UPDATE_FILE_RESPONSE,

    ENUM_MSG_TYPE_DEL_FILE_RESPEST, //删除常规文件
    ENUM_MSG_TYPE_DEL_FILE_RESPONSE,

    ENUM_MSG_TYPE_DOWNLOAD_FILE_RESPEST, //删除常规文件
    ENUM_MSG_TYPE_DOWNLOAD_FILE_RESPONSE,

    ENUM_MSG_TYPE_SHARE_FILE_RESPEST,   //共享文件
    ENUM_MSG_TYPE_SHARE_FILE_RESPONSE,

    ENUM_MSG_TYPE_SHARE_FILE_NOTE,
    ENUM_MSG_TYPE_SHARE_FILE_NOTE_RESPONSE,

    ENUM_MSG_TYPE_MOVE_FILE_RESPEST,   //移动文件
    ENUM_MSG_TYPE_MOVE_FILE_RESPONSE,
    ENUM_MSG_TYPE_MAX=0x00ffffff
};
//struct 的作用域只要包含头文件即可；！！

struct FileInfo
{
    char caFileName[64];    //这里都需要判断
    int iFileType;
};


typedef struct PDU
{
    uint uiPDULen_;
    uint uiMsgLen_;
    uint uiMsgType_;     //消息类型，不同的处理不同
    char caData[64];    //后面使用保存文件名
    int caMsg[];       //弹性数组，不占用内存，指向实际消息,当时为啥是int、？？？
}PDU;

PDU *mkPDU(uint uiMsgLen);   //声明，创建一个，并返回（类型初始化吧），我要的是实际的消息uiMsgLen+sizeof(PDU);
#endif // PROTOCOL_H
