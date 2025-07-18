#ifndef CONFIG_H
#define CONFIG_H

/* Version number */
#ifndef VERSION_NUMBER
#define VERSION_NUMBER "0.0.0-devel"
#endif

/* Supported config format */
#define IMAGE_LIST_CONFIG_FORMAT 1
#define IMAGE_CONFIG_FORMAT 5

/* Color of the background */
#define BACKGROUND_COLOR  Qt::black

/* Highlight color of installed OS */
#define INSTALLED_OS_BACKGROUND_COLOR  QColor(0xef,0xff,0xef)

/* Disable language selection */
#undef ENABLE_LANGUAGE_CHOOSER

/* Website launched when launching Arora */
#define HOMEPAGE  "http://www.raspberrypi.org/help/"

/* Location to download the list of available distributions from
 * Multiple lists can be specified by space separating the URLs */
#define DEFAULT_IMAGE_HOST "tezi.toradex.com"
#define DEFAULT_IMAGE_SERVER  "http://" DEFAULT_IMAGE_HOST "/image_list.json"
#define DEFAULT_3RDPARTY_IMAGE_SERVER  "http://" DEFAULT_IMAGE_HOST "/image_list_3rdparty.json"
#define DEFAULT_CI_IMAGE_SERVER  "http://" DEFAULT_IMAGE_HOST "/image_list_ci.json"

/* Keys in tezi_config.json */
#define TEZI_CONFIG_JSON_DEFAULT_FEED "show_default_feed"
#define TEZI_CONFIG_JSON_3RDPARTY_FEED "show_3rdparty_feed"

/* Size of recovery FAT partition in MB.
 * First partition starts at offset 1 MB (sector 2048)
 * If you want the second partition to start at offset 1024 MB, enter 1023 */
#define RESCUE_PARTITION_SIZE  63

/* Files that are currently on the FAT partition are normaaly saved to memory during
 * repartitioning.
 * If files they are larger than number of MB, try resizing the FAT partition instead */
#define MAXIMUM_BOOTFILES_SIZE  64

/* Partitioning settings */
#define PARTITION_ALIGNMENT  8192

/* Source media mount folder */
#define SRC_MOUNT_FOLDER "/run/media/src"

/* Temporary mount folder */
#define TEMP_MOUNT_FOLDER "/run/media/tmp"

/* Default curl command line options */
#define DOWNLOAD_COMMAND "curl --silent --show-error --connect-timeout 10 --continue-at - --speed-limit 1024 --speed-time 10 --retry 10"

/* Use named pipe to communicate data to md5sum */
#define MD5SUM_NAMEDPIPE "/var/volatile/md5sumpipe"

/* Use named pipe to communicate result of pipe viewer for progress to Qt UI */
#define PIPEVIEWER_NAMEDPIPE "/var/volatile/pvpipe"
#define PIPEVIEWER_COMMAND "pv -b -n 2>" PIPEVIEWER_NAMEDPIPE

/* NCM network */
#define NCM_ADDRESS "192.168.11"

#endif // CONFIG_H
