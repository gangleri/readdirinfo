/*
 * =====================================================================================
 *
 *       Filename:  readdirinfo.cc
 *
 *    Description: Node.js module that returns a list of info json objects for
 *                 files in a given dir
 *
 *        Version:  1.0
 *        Created:  10/11/13 13:34:43
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author: Alan Bradley 
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <node.h>
#include <v8.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <string>
#include <sys/stat.h>

using namespace std;
using namespace node;
using namespace v8;

struct DirInfo {
  string name;
  string type_name;
  unsigned int type;
  unsigned int size;
};

struct ReadDirInfo_req {
  uv_work_t req;
  string path;
  int num_info;
  DirInfo* info;
  Persistent<Function> callback;
};

void Worker(uv_work_t* req) {
  ReadDirInfo_req *request = static_cast<ReadDirInfo_req *>(req->data);
  struct dirent **list;
  struct stat buff;
  int n = scandir(request->path.c_str(), &list, 0, alphasort);

  request->info = new DirInfo[n];
  request->num_info = n;

  while (n--) {
    request->info[n].name = list[n]->d_name;
    string fullPath = request->path + "/" + request->info[n].name;

    stat(fullPath.c_str(), &buff);
    request->info[n].size = (unsigned int)(buff.st_size);

    switch(list[n]->d_type)  {
      case DT_FIFO:
        request->info[n].type_name = "FIFO";
        break;

      case DT_CHR:
        request->info[n].type_name = "Character";
        break;

      case DT_DIR:
        request->info[n].type_name = "Directory";
        break;

      case DT_BLK:
        request->info[n].type_name = "Block";
        break;

      case DT_REG:
        request->info[n].type_name = "Regular";
        break;

      case DT_LNK:
        request->info[n].type_name = "Link";
        break;

      case DT_SOCK:
        request->info[n].type_name = "Socket";
        break;
      case DT_WHT:
        request->info[n].type_name = "WHT";
        break;

      case DT_UNKNOWN:
      default:
        request->info[n].type_name = "Unknown";
        break;
    }

    free(list[n]);
  }
  free(list);
}

void After(uv_work_t* req, int status) {
  HandleScope scope;
  ReadDirInfo_req* request = static_cast<ReadDirInfo_req*>(req->data);

  Handle<Value> argv[2];
  argv[0] = Undefined();
  Local<Array> arr = Array::New();

  for(int i = 0; i < request->num_info; i++) {
    Local<Object> obj = Object::New();
    obj->Set(String::New("name"), String::New(request->info[i].name.c_str()));
    obj->Set(String::New("type"), String::New(request->info[i].type_name.c_str()));
    obj->Set(String::New("size"), Number::New(request->info[i].size));
    arr->Set(i,obj);
  }
  argv[1] = arr;

  TryCatch try_catch;

  request->callback->Call(Context::GetCurrent()->Global(), 2, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }

  request->callback.Dispose();

  delete request;
}

Handle<Value> ReadDirInfo(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2) return ThrowException(Exception::TypeError(String::New("You must provide a path and a callback"))); 
  if (!args[0]->IsString()) return ThrowException(Exception::TypeError(String::New("path must be a string")));
  if (!args[1]->IsFunction()) return ThrowException(Exception::TypeError(String::New("callback must be a function")));

  String::Utf8Value path(args[0]->ToString());
  ReadDirInfo_req  *req = new ReadDirInfo_req;
  req->req.data = req;
  req->path = strdup(*path);
  req->callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));

  uv_queue_work(uv_default_loop(), 
      &req->req, 
      Worker,
      After);

  return Undefined();
}

void init(Handle<Object> exports, Handle<Object>module) {
  module->Set(String::NewSymbol("exports"), 
      FunctionTemplate::New(ReadDirInfo)->GetFunction());
}

NODE_MODULE(readdirinfo, init)
