// Logtail implementation in C
//   just use logtail -f logfile to tail your logs
//   should survive log rotation
// xnicollet _at__ gmail.com
// This code is licensed in GPLv2.

#include <sstream>
#include <iostream>
#include <string>
#include <fstream>
#include <memory>

#include <sys/types.h> // C open
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h> // C read
#include <stdio.h>  // snprintf

#include "logtail.h"

void Logtail::usage() {
  std::cout << "usage: logtail -f [File]" << std::endl;
  std::cout << "  Will puts the end of the file" << std::endl;
}

Logtail::Logtail(char const *filename)
    : filename_(filename), file_(-1),
      offsetfile_(filename + std::string(".offset")), offset_fd_(-1),
      inode_(0), offset_(0) {
  stat_.st_ino = 0;
}

bool Logtail::openFile() {
  int file = open(filename_.c_str(), O_RDONLY);
  if (file >= 0) {
    file_ = file;
    return true;
  } else {
    std::cerr << "File " << filename_ << " cannot be read." << std::endl;
    return false;
  }
}

// -1 ok, other exit code
int Logtail::seek() {
  int ret = fstat(file_, &stat_);
  if (ret < 0) {
    std::cerr << "Cannot get " << filename_ << " file size." << std::endl;
    return 65;
  }
  if (inode_ == stat_.st_ino) {
    if (offset_ == stat_.st_size) {
      return 0;
    }
  }
  if ( !parse_offset() ) { return -1; }

  if (offset_ > stat_.st_size) {
    offset_ = 0;
    std::cerr << "***************" << std::endl;
    std::cerr <<  "*** WARNING ***: Log file " << filename_
              << " is smaller than last time checked!" << std::endl;
    std::cerr <<  "*************** This could indicate tampering." << std::endl;
  }

  if (inode_ != stat_.st_ino || offset_ > stat_.st_size) {
    // std::cerr << "debug inode_: " << inode_ << " st_ino: " << stat_.st_ino << std::endl;
    offset_ = 0;
  }

  off_t seek_ret = lseek(file_, offset_, SEEK_SET);
  if (seek_ret == static_cast<off_t>(-1)) {
    std::cerr << "can't lseek in the file" << std::endl;
    return 66;
  }
  return -1;
}

bool Logtail::save_offset() {
  off_t new_offset = lseek(file_, 0, SEEK_CUR);
  if (new_offset == static_cast<off_t>(-1)) {
    std::cerr << "can't get file offset" << std::endl;
    return false;
  }
  if (close(file_) < 0) {
    std::cerr << "can't close " << filename_ << std::endl;
    return false;
  }

  // basically: > offsetfile_ (aka file.offset, offset_fd_)
  if (offset_fd_ > 0) {
    close(offset_fd_);
  }
  if ( creat_offset() < 0 ) {
    std::cerr << "Could not create " << offsetfile_ << std::endl;
    return false;
  }

  // just write the values
  // TODO: something more C++
  int to_write = snprintf(offset_buffer_, sizeof(offset_buffer_), "%ld\n%ld\n", stat_.st_ino, new_offset);
  offset_buffer_[to_write] = '\0';
  char *wr_buff = offset_buffer_;
  int nb_try = 0;
  int written;
  do {
    ++nb_try;
    written = write(offset_fd_, wr_buff, to_write);
    if (written<0) {
      continue;
    }
    wr_buff += written;
    to_write -= written;
  } while (to_write > 0 && nb_try < 3);

  if (to_write > 0) {
    std::cerr << "could not write the offset file" << std::endl;
    return false;
  }
  close(offset_fd_);

  return true;
}

bool Logtail::tail_core() {
  const int read_buffer_size = 4096 * 2; // let's read 2 pages at once
  static char read_buffer[read_buffer_size]; 
  int have_read;
  do  {
    have_read = read(file_, read_buffer, read_buffer_size);
    int to_write = have_read;
    char *buffer = read_buffer;
    for(int nb_try=0; nb_try<3 && to_write>0; ++nb_try) {
      int written = write(1, buffer, have_read);
      if (written >= 0) {
        to_write -= written;
        buffer += written;
      }
    }
    if (to_write > 0) {
      std::cerr << "Could not write the output." << std::endl;
      return false;
    }
  } while (have_read > 0);
  return true;
}

int Logtail::creat_offset() {
  offset_fd_ = creat(offsetfile_.c_str(), 00644);
  return offset_fd_;
}

// return true if we have read offset, false if not
bool Logtail::parse_offset() { 
  offset_fd_ = open(offsetfile_.c_str(), O_RDONLY);
  if (offset_fd_ <= 0) { return false; } // We don't have an offset: keep defaults

  ssize_t nb_read = read(offset_fd_, offset_buffer_, sizeof(offset_buffer_)-1);
  if (nb_read <= 0) {
    std::cerr << "can't read " << offsetfile_ << ". Going as it does not exist" << std::endl;
    return false;
  }
  offset_buffer_[nb_read] = '\0'; // protect our char *against buffer overflow

  std::string test(offset_buffer_);
  std::istringstream ssm( offset_buffer_ );
  ssm >> inode_ >> offset_;

  // std::cerr << "read from offsetfile: inode: " << inode_ << " offset: " << offset_ << std::endl; // debug
  return true;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    Logtail::usage();
    return 1;
  } else {
    const std::string opt {argv[1]};
    if (opt != "-f") {
      Logtail::usage();
      return 2;
    }
  }
  Logtail logtail(argv[2]);

  if ( !logtail.openFile() ) { return 66; }
  int s = logtail.seek();
  if (s >= 0) {
    return s;
  }
  if (!logtail.tail_core()) {
    return 2;
  }
  logtail.save_offset();

  return 0;
}

// vim: set tabstop=2 sw=2 list expandtab:
