#include <string.h>
#include "fat32.h"
#include "disk.h"

BPB_struct *bpb;

int tf_fseek(TFFile *fp, int32_t base, long offset) {
    long pos = base+offset;
    if (pos >= fp->size) return -1;
    return tf_unsafe_fseek(fp, base, offset);
}

int read_sector(uint8_t *data, uint32_t sector) {
    FILE *fp;
    fp = fopen("test.fat32", "r+b");
    fseek(fp, sector*512, 0);
    fread(data, 1, 512, fp);
    fclose(fp);
    return 0;
}

int write_sector(uint8_t *data, uint32_t blocknum) {
    FILE *fp;
    fp = fopen("test.fat32", "r+");
    fseek(fp, blocknum*512, 0);
    fwrite(data, 1, 512, fp);
    fclose(fp);
    return 0;
}

int tf_store() {
    tf_info.sectorFlags &= ~TF_FLAG_DIRTY;
    return write_sector( tf_info.buffer, tf_info.currentSector );
}

int tf_fetch(uint32_t sector) {
    int rc=0;
    // Don't actually do the fetch if we already have it in memory
    if(sector == tf_info.currentSector){
        return 0;
    }
    
    // If the sector we already have prefetched is dirty, write it before reading out the new one
    if(tf_info.sectorFlags & TF_FLAG_DIRTY) {
        rc |= tf_store();
    }

    // Do the read, pass up the error flag
    rc |= read_sector( tf_info.buffer, sector );
    if(!rc) tf_info.currentSector = sector;
    return rc;
}

void tf_release_handle(TFFile *fp) {
    fp->flags &= ~TF_FLAG_OPEN;
}

int tf_fflush(TFFile *fp) {
    int rc = 0;
    TFFile *dir;
    FatFileEntry entry;
    uint8_t *filename=entry.msdos.filename;

    if(!(fp->flags & TF_FLAG_DIRTY)) return 0;

    // First write any pending data to disk
    if(tf_info.sectorFlags & TF_FLAG_DIRTY) {
        rc = tf_store();
    }
    // Now go modify the directory entry for this file to reflect changes in the file's size
    // (If they occurred)
    if(fp->flags & TF_FLAG_SIZECHANGED) {

        if(fp->attributes & 0x10) {
            // TODO Deal with changes in the root directory size here
        }
        else {
            // Open the parent directory
            dir = tf_parent(fp->filename, "r+", 0);
            if (dir == (void*)-1)            {
                return -1;
            }
            
            filename = (uint8_t*)strrchr((char const*)fp->filename, '/');

        
            // Seek to the entry we want to modify and pull it from disk
            tf_find_file(dir, filename+1);
            tf_fread((uint8_t*)&entry, sizeof(FatFileEntry), dir);
            tf_fseek(dir, -sizeof(FatFileEntry), dir->pos);
            
            // Modify the entry in place to reflect the new file size
            entry.msdos.fileSize = fp->size-1; 
            tf_fwrite((uint8_t*)&entry, sizeof(FatFileEntry), 1, dir); // Write fatfile entry back to disk
            tf_fclose(dir);
        }
        fp->flags &= ~TF_FLAG_SIZECHANGED;
    }
    
    fp->flags &= ~TF_FLAG_DIRTY;
    return rc;
}

int tf_fread(uint8_t *dest, int size, TFFile *fp) {
    uint32_t sector;
    while(size > 0) {
        sector = tf_first_sector(fp->currentCluster) + (fp->currentByte / 512);
        tf_fetch(sector);       // wtfo?  i know this is cached, but why!?
        //printHex(&tf_info.buffer[fp->currentByte % 512], 1);
        *dest++ = tf_info.buffer[fp->currentByte % 512];
        size--;
        if(fp->attributes & TF_ATTR_DIRECTORY) {
            //dbg_printf("READING DIRECTORY");
            if(tf_fseek(fp, 0, fp->pos+1)) {
                return -1;
            }
        } else {
        if(tf_fseek(fp, 0, fp->pos +1)) {
            return -1;    
        }
        }
    }
    return 0;
}

TFFile *tf_fnopen(uint8_t *filename, const uint8_t *mode, int n) {
    // Request a new file handle from the system
    TFFile *fp = tf_get_free_handle();
    uint8_t myfile[256];
    uint8_t *temp_filename = myfile;
    uint32_t cluster;

    if (fp == NULL)
        return (TFFile*)-1;

    strncpy(myfile, filename, n);
    myfile[n] = 0;
    fp->currentCluster=2;           // FIXME: this is likely supposed to be the first cluster of the Root directory...
                                    // however, this is set in the BPB...
    fp->startCluster=2;
    fp->parentStartCluster=0xffffffff;
    fp->currentClusterIdx=0;
    fp->currentSector=0;
    fp->currentByte=0;
    fp->attributes = TF_ATTR_DIRECTORY;
    fp->pos=0;
    fp->flags |= TF_FLAG_ROOT;
    fp->size = 0xffffffff;
    //fp->size=tf_info.rootDirectorySize;
    fp->mode=TF_MODE_READ | TF_MODE_WRITE | TF_MODE_OVERWRITE;


    
    while(temp_filename != NULL) {
        temp_filename = tf_walk(temp_filename, fp);
        if(fp->flags == 0xff) {
            tf_release_handle(fp);
            return NULL;
        }
    }
    
    if(strchr(mode, 'r')) {
        fp->mode |= TF_MODE_READ;
    }
    if(strchr(mode, 'a')) { 
        tf_unsafe_fseek(fp, fp->size, 0);
        fp->mode |= TF_MODE_WRITE | TF_MODE_OVERWRITE;
    }
    if(strchr(mode, '+')) fp->mode |= TF_MODE_OVERWRITE | TF_MODE_WRITE;
    if(strchr(mode, 'w')) {
        /* Opened for writing. Truncate file only if it's not a directory*/
        if (!(fp->attributes & TF_ATTR_DIRECTORY)) {
            fp->size = 0;
            tf_unsafe_fseek(fp, 0, 0);
            /* Free the clusterchain starting with the second one if the file
             * uses more than one */
            if ((cluster = tf_get_fat_entry(fp->startCluster)) != TF_MARK_EOC32) {
                tf_free_clusterchain(cluster);
                tf_set_fat_entry(fp->startCluster, TF_MARK_EOC32);
            }
        }
        fp->mode |= TF_MODE_WRITE;
    }

    strncpy(fp->filename, myfile, n);
         
    fp->filename[n] = 0;
    return fp;
}

int tf_mkdir(uint8_t *filename, int mkParents) {
    // FIXME: verify that we can't create multiples of the same one.
    // FIXME: figure out how the root directory location is determined.
    uint8_t orig_fn[ TF_MAX_PATH ];
    TFFile *fp;
    FatFileEntry entry, blank;

    uint32_t psc;
    uint32_t cluster;
    uint8_t *temp;    

    strncpy( orig_fn, filename, TF_MAX_PATH-1 );
    orig_fn[ TF_MAX_PATH-1 ] = 0;
    
    memset(&blank, 0, sizeof(FatFileEntry));
    
    fp = tf_fopen(filename, "r");
    if (fp)  // if not NULL, the filename already exists.
    {
        tf_fclose(fp);
        tf_release_handle(fp);
        if (mkParents)
        {
            tf_printf("\r\n[DEBUG-tf_mkdir] Skipping creation of existing directory.");
            return 0;
        }
        dbg_printf("\r\n[DEBUG-tf_mkdir] Hey there, duffy, DUPLICATES are not allowed.");
        return 1;
    }
    
    dbg_printf("\r\n[DEBUG-tf_mkdir] The directory does not currently exist... Creating now.  %s", 
               filename);
    fp = tf_parent(filename, "r+", mkParents);
    if (!fp)
    {
        dbg_printf("\r\n[DEBUG-tf_mkdir] Parent Directory doesn't exist.");
        return 1;
    }
    
    dbg_printf("\r\n[DEBUG-tf_mkdir] Creating new directory: '%s'", filename);
    // Now we have the directory in which we want to create the file, open for overwrite
    do {
        tf_fread((uint8_t*)&entry, sizeof(FatFileEntry), fp);
        tf_printf("Skipping existing directory entry... %d\n", fp->pos);
    } while(entry.msdos.filename[0] != '\x00');
    // Back up one entry, this is where we put the new filename entry
    tf_fseek(fp, -sizeof(FatFileEntry), fp->pos);
    
    // go find some space for our new friend
    cluster = tf_find_free_cluster();
    tf_set_fat_entry(cluster, TF_MARK_EOC32); // Marks the new cluster as the last one (but no longer free)
    
    // set up our new directory entry
    // TODO shorten these entries with memset
    entry.msdos.attributes = TF_ATTR_DIRECTORY ;
    entry.msdos.creationTimeMs = 0x25;
    entry.msdos.creationTime = 0x7e3c;
    entry.msdos.creationDate = 0x4262;
    entry.msdos.lastAccessTime = 0x4262;
    entry.msdos.eaIndex = (cluster >> 16) & 0xffff;
    entry.msdos.modifiedTime = 0x7e3c;
    entry.msdos.modifiedDate = 0x4262;
    entry.msdos.firstCluster = cluster & 0xffff;
    entry.msdos.fileSize = 0;
    temp = strrchr(filename, '/')+1;
    dbg_printf("\r\n[DEBUG-tf_mkdir] DIRECTORY NAME CONVERSION: %s", temp);
    tf_choose_sfn(entry.msdos.filename, temp, fp);
    dbg_printf("\r\n==== tf_mkdir: SFN: %s", entry.msdos.filename);
    tf_place_lfn_chain(fp, temp, entry.msdos.filename);
    //tf_choose_sfn(entry.msdos.filename, temp, fp);
    //tf_shorten_filename(entry.msdos.filename, temp, 1);
    //printf("\r\n==== tf_mkdir: SFN: %s", entry.msdos.filename);
    //    dbg_printf("  3 mkdir: attr: %x ", entry.msdos.attributes); // attribute byte still getting whacked.
    //entry.msdos.attributes = TF_ATTR_DIRECTORY ;
    //    dbg_printf("  4 mkdir: attr: %x ", entry.msdos.attributes);
    tf_fwrite((uint8_t*)&entry, sizeof(FatFileEntry), 1, fp);
    
    psc = fp->startCluster; // store this for later
    
    // placing a 0 at the end of the FAT
    tf_fwrite((uint8_t*)&blank, sizeof(FatFileEntry), 1, fp);
    tf_fclose(fp);
    tf_release_handle(fp);
    
    dbg_printf("\r\n  initializing directory entry: %s", orig_fn);
    fp = tf_fopen(orig_fn, "w");
    
    // set up .
    memcpy( entry.msdos.filename, ".          ", 11 );
    //entry.msdos.attributes = TF_ATTR_DIRECTORY;
    //entry.msdos.firstCluster = cluster & 0xffff    
    tf_fwrite((uint8_t*)&entry, sizeof(FatFileEntry), 1, fp);
    
    // set up ..
    memcpy( entry.msdos.filename, "..         ", 11 );
    //entry.msdos.attributes = TF_ATTR_DIRECTORY;
    //entry.msdos.firstCluster = cluster & 0xffff
    tf_fwrite((uint8_t*)&entry, sizeof(FatFileEntry), 1, fp);

    // placing a 0 at the end of the FAT
    tf_fwrite((uint8_t*)&blank, sizeof(FatFileEntry), 1, fp);
    
    tf_fclose(fp);
    tf_release_handle(fp);
    return 0;
}


//**** COMPLETE UNTESTED tf_listdir ****//
// this may be better served by simply opening the directory directly through C2
// parsing is not a huge deal...
// returns 1 when valid entry, 0 when done.
int tf_listdir(uint8_t *filename, FatFileEntry* entry, TFFile **fp) {
    // May Require a terminating "/."
    // FIXME: What do we do with the results?!  perhaps take in a fp and assume
    //  that if not NULL, it's already in the middle of a listdir!  if NULL
    //  we do the tf_parent thing and then start over.  if not, we return the 
    //  next FatFileEntry almost like a callback...  and return NULL when we've
    //  reached the end.  almost like.. gulp... strtok.  ugh.  maybe not.  
    //  still, it may suck less than other things... i mean, by nature, listdir 
    //  is inherently dynamic in size, and we have no re/malloc.
    uint32_t cluster;
    uint8_t *temp;    

    if (*fp == NULL)
        *fp = tf_parent(filename, "r", 0);
    
    if(!*fp) return 1;
    // do magic here.
    while (1)
    {
        tf_fread((uint8_t*)entry, sizeof(FatFileEntry), *fp);
        switch (((uint8_t*)entry)[0]){
        case 0x05:
            // pending delete files under some implementations.  ignore it.
            break;
        case 0xe5:
            // freespace (deleted file)
            break;
        case 0x2e:
            // '.' or '..'
            break;
        case 0x00:
            // no further entries exist, and this one is available
            tf_fclose(*fp);
            *fp = NULL;
            return 0;
            
        default:
            return 1;
        }
    }
        
    return 0;
}

void tf_choose_sfn(uint8_t *dest, uint8_t *src, TFFile *fp){
    int results, num = 1;
    TFFile xfile;
	uint8_t *temp;
    // throwaway fp that doesn't muck with the original
    memcpy( &xfile, fp, sizeof(TFFile) );
    
    while (1)
    {
        results = tf_shorten_filename( dest, src, num );
        switch (results)
        {
        case 0: // ok
            // does the file collide with the current directory?
            //tf_fseek(xfile, 0, 0);
            memcpy(temp, dest, 8);
            memcpy(temp+9, dest+8, 3);
            temp[8] = '.';
            temp[12] = 0;
            
            if (0 > tf_find_file( &xfile, temp ) )
            {
                //tf_
                return;
                break;
            }
            //tf
            num++;
            break;
            
            
        case -1: // error
            return;
        }
    }
}

int tf_recover(char *filename) {
	TFFile *fp;
	fp = tf_fopen(filename, "r");
    BootEntry* disk = readFileSystem(fd);

    //(int fd, BootEntry* disk, char *filename){
    unsigned int nEntries=0;
    unsigned int currCluster = disk->BPB_RootClus;
    unsigned int totalPossibleEntry = (disk->BPB_SecPerClus * disk->BPB_BytsPerSec)/sizeof(DirEntry);

    //unsigned char* file_content  = mmap(NULL , fs.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    int fileCount=0;
    // store first occurence
    int nEntries1=0;
    int currCluster1=0;
    DirEntry* dirEntry1;

    do{
        DirEntry* dirEntry = getclusterPtr(file_content,disk,currCluster);
        for(unsigned int m=0;m<totalPossibleEntry;m++){
            if (dirEntry->DIR_Attr == 0x00){        // no more dirEntry after this
                break;
            }
            if(isDirectory(dirEntry)==0 && dirEntry->DIR_Name[0] == 0xe5){      // do not show deleted files
                if (dirEntry->DIR_Name[1]==filename[1]){
                    char *recFilename = getfilename(dirEntry);
                    if(strcmp(recFilename+1,filename+1)==0){
                        if (shaFile){ // if sha is provided to recover the file
                            bool shaMatched = checkSHA(getShaOfFileContent(disk, dirEntry, file_content), shaFile);
                            if (shaMatched){
                                fileCount=1;
                                nEntries1=nEntries;
                                dirEntry1=dirEntry;
                                currCluster1=currCluster;
                            }
                        }
                        else{
                            // more than one file with same name except first character
                            if (fileCount<1){
                                nEntries1=nEntries;
                                dirEntry1=dirEntry;
                                currCluster1=currCluster;
                                fileCount++;
                            }
                            else{
                                printf("%s: multiple candidates found\n",filename);
                                fflush(stdout);
                                return 1;
                            }
                        }
                    }
                }
            }
            dirEntry++;
            nEntries+=sizeof(DirEntry);
        }
        unsigned int *fat = (unsigned int*)(file_content + disk->BPB_RsvdSecCnt*disk->BPB_BytsPerSec + 4*currCluster);
        if(*fat >= 0x0ffffff8 || *fat==0x00){
            break;
        }
        currCluster=*fat;
    } while(1);

    if (fileCount==1){
        updateRootDir(file_content, disk, filename, nEntries1);

        int n_Clusters = nOfContiguousCluster(disk, dirEntry1);
        if (n_Clusters>1){
            int startCluster = dirEntry1->DIR_FstClusHI << 16 | dirEntry1->DIR_FstClusLO;
            for(int i=1;i<n_Clusters;i++){
                updateFat(file_content , disk, startCluster, startCluster+1);
                startCluster++;
            }
            updateFat(file_content , disk, startCluster, 0x0ffffff8);
        }
        else
            updateFat(file_content , disk, currCluster1+1, 0x0ffffff8);
        unmapDisk(file_content, fs.st_size);

        if (shaFile)
            printf("%s: successfully recovered with SHA-1\n",filename);
        else
            printf("%s: successfully recovered\n",filename);
        fflush(stdout);
        return 1;
    }
    return -1;
}

int tf_remove(uint8_t *filename) {
    TFFile *fp;
    FatFileEntry entry;
    int rc;
    uint32_t startCluster;

    // Sanity check
    fp = tf_fopen(filename, "r");
    if(fp == NULL) return -1; // return an error if we're removing a file that doesn't exist
    startCluster = fp->startCluster; // Remember first cluster of the file so we can remove the clusterchain
    tf_fclose(fp);

    // TODO Don't leave an orphaned LFN
    fp = tf_parent(filename, "r+", 0);
    rc = tf_find_file(fp, (strrchr((uint8_t *)filename, '/')+1));
    if(!rc) {
        while(1) {
            rc = tf_fseek(fp, sizeof(FatFileEntry), fp->pos);
            if(rc) break;
            tf_fread((uint8_t*)&entry, sizeof(FatFileEntry), fp); // Read one entry ahead
            tf_fseek(fp, -((int32_t)2*sizeof(FatFileEntry)), fp->pos);
            tf_fwrite((uint8_t*)&entry, sizeof(FatFileEntry), 1, fp);
            if(entry.msdos.filename[0] == 0) break;
        }
        fp->size-=sizeof(FatFileEntry);
        fp->flags |= TF_FLAG_SIZECHANGED; 
    }
    tf_fclose(fp);
    tf_free_clusterchain(startCluster); // Free the data associated with the file

    return 0;

}

uint8_t *tf_create_lfn_entry(uint8_t *filename, FatFileEntry *entry) {
    int i, done=0;
    for(i=0; i<5; i++) {
        if (!done)
            entry->lfn.name1[i] = (unsigned short) *(filename);
        else
            entry->lfn.name1[i] = 0xffff;
        if(*filename++ == '\x00')
            done = 1;
    }
    for(i=0; i<6; i++) {
        if (!done)
            entry->lfn.name2[i] = (unsigned short) *(filename);
        else
            entry->lfn.name2[i] = 0xffff;
        if(*filename++ == '\x00')
            done = 1;
    }
    for(i=0; i<2; i++) {
        if (!done)
            entry->lfn.name3[i] = (unsigned short) *(filename);
        else
            entry->lfn.name3[i] = 0xffff;
        if(*filename++ == '\x00')
            done = 1;
    }
        
    entry->lfn.attributes = 0x0f;
    entry->lfn.reserved = 0;
    entry->lfn.firstCluster = 0;
    if(done) return NULL;
    if(*filename) return filename;
    else return NULL;
}

int tf_fclose(TFFile *fp) {
    int rc;

    rc =  tf_fflush(fp);
    fp->flags &= ~TF_FLAG_OPEN; // Mark the file as available for the system to use
    // FIXME: is there any reason not to release the handle here?
    return rc;
}

int tf_create(uint8_t *filename) {
    TFFile *fp = tf_parent(filename, "r", 0);
    FatFileEntry entry;
    uint32_t cluster;
    uint8_t *temp;    
    if(!fp) return 1;
    tf_fclose(fp);
    fp = tf_parent(filename, "r+", 0);
    // Now we have the directory in which we want to create the file, open for overwrite
    do {
        //"seek" to the end
        tf_fread((uint8_t*)&entry, sizeof(FatFileEntry), fp);
    } while(entry.msdos.filename[0] != '\x00');
    // Back up one entry, this is where we put the new filename entry
    tf_fseek(fp, -sizeof(FatFileEntry), fp->pos);
    cluster = tf_find_free_cluster();
    tf_set_fat_entry(cluster, TF_MARK_EOC32); // Marks the new cluster as the last one (but no longer free)
    // TODO shorten these entries with memset
    entry.msdos.attributes = 0;
    entry.msdos.creationTimeMs = 0x25;
    entry.msdos.creationTime = 0x7e3c;
    entry.msdos.creationDate = 0x4262;
    entry.msdos.lastAccessTime = 0x4262;
    entry.msdos.eaIndex = (cluster >> 16) & 0xffff;
    entry.msdos.modifiedTime = 0x7e3c;
    entry.msdos.modifiedDate = 0x4262;
    entry.msdos.firstCluster = cluster & 0xffff;
    entry.msdos.fileSize = 0;
    temp = strrchr(filename, '/')+1;
    tf_choose_sfn(entry.msdos.filename, temp, fp);
    tf_place_lfn_chain(fp, temp, entry.msdos.filename);
    //tf_choose_sfn(entry.msdos.filename, temp, fp);
    //tf_shorten_filename(entry.msdos.filename, temp);
    //printf("\r\n==== tf_create: SFN: %s", entry.msdos.filename);
    tf_fwrite((uint8_t*)&entry, sizeof(FatFileEntry), 1, fp);
    memset(&entry, 0, sizeof(FatFileEntry));
    //entry.msdos.filename[0] = '\x00';
    tf_fwrite((uint8_t*)&entry, sizeof(FatFileEntry), 1, fp);
    tf_fclose(fp);
    return 0;
}

TFFile *tf_fopen(uint8_t *filename, const uint8_t *mode) {
    TFFile *fp;

    fp = tf_fnopen(filename, mode, strlen(filename));
    if(fp == NULL) {
        if(strchr(mode, '+') || strchr(mode, 'w') || strchr(mode, 'a')) {
              tf_create(filename); 
        }    
        return tf_fnopen(filename, mode, strlen(filename));
    }
    return fp;
}

int tf_init() {
    uint32_t fat_size, root_dir_sectors, data_sectors, cluster_count, temp;
    TFFile *fp;
    FatFileEntry e;

    // Initialize the runtime portion of the TFInfo structure, and read sec0
    tf_info.currentSector = -1;
    tf_info.sectorFlags = 0;
    tf_fetch(0);

    // Cast to a BPB, so we can extract relevant data
    bpb = (BPB_struct *) tf_info.buffer;
    
    /* Some sanity checks to make sure we're really dealing with FAT here
     * see fatgen103.pdf pg. 9ff. for details */

    /* BS_jmpBoot needs to contain specific instructions */
    if (!(bpb->BS_JumpBoot[0] == 0xEB && bpb->BS_JumpBoot[2] == 0x90) && !(bpb->BS_JumpBoot[0] == 0xE9)){
        return -1;
    }

    /* Only specific bytes per sector values are allowed
     * FIXME: Only 512 bytes are supported by thinfat at the moment */
    if (bpb->BytesPerSector != 512){
        return -1;
    }

    if (bpb->ReservedSectorCount == 0){
        return -1;
    }

    /* Valid media types */
    if ((bpb->Media != 0xF0) && ((bpb->Media < 0xF8) || (bpb->Media > 0xFF))){
        return -1;
    }

    // See the FAT32 SPEC for how this is all computed
    fat_size                  = (bpb->FATSize16 != 0) ? bpb->FATSize16 : bpb->fat32.FATSize;
    root_dir_sectors          = ((bpb->RootEntryCount*32) + (bpb->BytesPerSector-1))/(512); // The 512 here is a hardcoded bpb->bytesPerSector (TODO: Replace /,* with shifts?)
    tf_info.totalSectors      = (bpb->TotalSectors16 != 0) ? bpb->TotalSectors16 : bpb->TotalSectors32;
    data_sectors              = tf_info.totalSectors - (bpb->ReservedSectorCount + (bpb->NumFATs * fat_size) + root_dir_sectors);
    tf_info.sectorsPerCluster = bpb->SectorsPerCluster;
    cluster_count             = data_sectors/tf_info.sectorsPerCluster;
    tf_info.reservedSectors   = bpb->ReservedSectorCount;
    tf_info.firstDataSector    = bpb->ReservedSectorCount + (bpb->NumFATs * fat_size) + root_dir_sectors;
    
    // Now that we know the total count of clusters, we can compute the FAT type
    if(cluster_count < 65525){
        return -1;
    }
    else tf_info.type = TF_TYPE_FAT32;

    // TODO ADD SANITY CHECKING HERE (CHECK THE BOOT SIGNATURE, ETC... ETC...)
    tf_info.rootDirectorySize = 0xffffffff;
    temp = 0;

    // Like recording the root directory size!
    // TODO, THis probably isn't necessary.  Remove later
    fp = tf_fopen("/", "r");
    do {
        temp += sizeof(FatFileEntry);
        tf_fread((uint8_t*)&e, sizeof(FatFileEntry), fp);
    } while(e.msdos.filename[0] != '\x00');
    tf_fclose(fp);
    tf_info.rootDirectorySize = temp;
    
    tf_fclose(fp);
    tf_release_handle(fp);
    return 0;    
}