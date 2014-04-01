#ifndef __LOGTAIL_H__
#define __LOGTAIL_H__

#include <string>

class Logtail {
public:
  static void usage();

  Logtail(char const *filename);
  bool openFile();

  // -1 ok, other exit code
  int seek();
  bool save_offset();
  bool tail_core();

private:
  int creat_offset();
  // return true if we have read offset, false if not
  bool parse_offset();

  std::string filename_;
  int file_;
  std::string offsetfile_;
  int offset_fd_;
  ino_t inode_; 
  off_t offset_;
  struct stat stat_;
  char offset_buffer_[1024]; // for parsing and saving
};

#endif // __LOGTAIL_H__
// vim: set tabstop=2 sw=2 list expandtab:
