Google Drive File System
========================

**GDFS** is a FUSE based filesystem written in C++, that lets you mount your Google Drive account in Linux.

### Features:
- Fully read-write filesystem.
- Supports regular files, directories, hard links, symbolic links, character files, FIFO files, socket files and block files.
- Supports boot-time mounting
- Supports High Availability
- File Names:
  - Supports unicode file names like äöü.
  - Supports '/' in file names, by replacing it with ‘_’ (‘/’ is used as path component separator in Linux).
  - Supports files with the same file name under the same parent directory, by renaming them to 'filename_1', 'filename_2' and so on. 
- File permissions:
  - Default permission for a file is 0644 and for a directory is 0755.
  - Default uid/gid of a file is the uid/gid of the owner of the file.
  - Support for sticky bit in file permissions (if set, only owner can delete/rename).
- Logging:
  - The default location where the logs are stored is */opt/gdfs/gdfs.log*
  - User can use any of the logging levels like DEBUG, INFO, WARNING, ERROR and FATAL, by modifying the *gdfs.log.level* parameter in the GDFS configuration file stored at */opt/gdfs/gdfs.conf*.
  - User can also change the location of the log file by modifying the *gdfs.log.path* parameter in the above configuration file.
- Support for Google Docs
  - GDFS supports Google Documents, Google Spreadsheets, Google Drawings and Google Presentations.
  - They shall be exported as *pdf* files with read-only file access.
- Request Queue
  - Every HTTP request to Google Drive API is handled through a Request Queue.
  - It uses a worker pool to complete each and every request in a *first-come-first-serve* manner.
  - Optimizations are done at the request level to minimize the number of requests sent *(like merging multiple requests, deleteing unnecessary requests)* in the request queue at any time.
- File Cache
  - A file cache is maintained to store file metadata as well as the actual file data.
  - File metadata in the file cache is invalidated only after 1 minute.
  - Files are cached as a list of pages, where each page can be of arbitrary size.
  - Every READ and WRITE request to a file is passed through the file cache.
  - Files are uploaded to Google Drive in chunks (10MB chunks) to restrict memory usage for large file uploads. File uploads are handled by the worker pool.
- Security
  - By default, access to the mount directory is restricted to the user who mounted GDFS.
  - If you need to allow access for others, modify the *gdfs.allow.others* parameter to *yes* in the GDFS configuration file.
  - If you need to allow access for root user only, modify the *gdfs.allow.root* parameter to *yes* in the GDFS configuration file.
  - GDFS uses the latest Google Drive v3 API.
  - GDFS creates a */opt/gdfs/gdfs.auth* file, which is used to authenticate with Google Drive API. This file has permissions set to 0600.

### Installation
GDFS supports installation through RPM packagament tools like Yum or Zypper, with support for operating systems like OpenSUSE 13.2, OpenSUSE 13.1, RHEL 7, Fedora 23, Fedora 22 and Centos 7. In case you are using a different operating system, use the traditional package installation method in Linux *(./configure, make, sudo make install)* to install GDFS.

```sh
$ zypper ar http://download.opensuse.org/repositories/home:/robinthomas/openSUSE_13.2/ robin
$ sudo zypper install gdfs
```

If you are using a different operating system, check out [Supported Operating Systems](http://download.opensuse.org/repositories/home:/robinthomas) to find the repository link for your operating system. 

Set the user who shall be mounting GDFS, by modifying the *gdfs.mount.user* in the GDFS configuration file. If no user is set, only root user can mount GDFS.

Run the **gauth** tool in GDFS as root user. It authenticates GDFS with your Google Drive account using OAuth2.0 protocol.

```sh
$ gauth
```

### Usage
GDFS is installed as a systemd service. Hence you can use the **systemctl** command to start/stop GDFS. In case you are using an operating system with no systemd support, use the bash script */opt/gdfs/gdfs.sh* to start/stop GDFS.

```sh
$ systemctl status gdfs
$ systemctl start gdfs
$ systemctl status gdfs
```

To enable *boot-time* mounting, run:
```sh
$ systemctl enable gdfs
```

To stop GDFS, run:
```sh
$ systemctl stop gdfs
```

### Support
GDFS is still a project in development. If you notice any issue, please open an [issue](https://github.com/robin-thomas/GDFS/issues) on Github.

If you have any questions, suggestions or feedback, feel free to send an email to <robinthomas17@gmail.com>.

Want to contribute? Great! Fork, edit and push!