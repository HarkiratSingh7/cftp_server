# CFTP-SERVER Documentation (Work In Progress)

## Demo
### Using LFTP (TLS FTP)
```
lftp -e "set ssl:verify-certificate no; set ftp:ssl-force true; set ftp:ssl-auth TLS; set ftp:ssl-protect-data true; open ftp://honey@localhost"
lftp honey@localhost:/> ls
-rw-r--r--  1 honey    honey     104857600 Oct 17 02:50 bigfile
lftp honey@localhost:/> put bigfile -o bigfile2
104857600 bytes transferred                   
lftp honey@localhost:/> ls
-rw-r--r--  1 honey    honey     104857600 Oct 17 02:50 bigfile2
-rw-r--r--  1 honey    honey     104857600 Oct 17 02:50 bigfile
lftp honey@localhost:/> get bigfile2 -o bigfile3
104857600 bytes transferred                     
lftp honey@localhost:/> ls
-rw-r--r--  1 honey    honey     104857600 Oct 17 02:50 bigfile2
-rw-r--r--  1 honey    honey     104857600 Oct 17 02:50 bigfile
lftp honey@localhost:/> quit
```