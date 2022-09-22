#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>

void _ls(const char *dir,int op_a,int op_l)
{
	//Here we will list the directory
	struct dirent *d;
	DIR *dh = opendir(dir);
	if (!dh)
	{
		if (errno = ENOENT)
		{
			//If the directory is not found
			perror("Directory doesn't exist");
		}
		else
		{
			//If the directory is not readable then throw error and exit
			perror("Unable to read directory");
		}
		exit(EXIT_FAILURE);
	}
	//While the next entry is not readable we will print directory files
	while ((d = readdir(dh)) != NULL)
	{
		//If hidden files are found we continue
		if (!op_a && d->d_name[0] == '.')
			continue;
		printf("%s  ", d->d_name);
		if(op_l) printf("\n");
	}
	if(!op_l)
	printf("\n");
}
int main(int argc, const char *argv[])
{
    int escolha;
    char s[100];

    printf("Bem vindo ao trabalho 2 de FSO!\n");

    while(1){
    printf("\n1. ls\n2. cd\n3. sair\nDigite a escolha: ");
    scanf("%d", &escolha);


        switch(escolha){
            case 1:
                if (argc == 1)
                {
                    _ls(".",0,0);
                }
                else if (argc == 2)
                {
                    if (argv[1][0] == '-')
                    {
                        //Checking if option is passed
                        //Options supporting: a, l
                        int op_a = 0, op_l = 0;
                        char *p = (char*)(argv[1] + 1);
                        while(*p){
                            if(*p == 'a') op_a = 1;
                            else if(*p == 'l') op_l = 1;
                            else{
                                perror("Option not available");
                                exit(EXIT_FAILURE);
                            }
                            p++;
                        }
                        _ls(".",op_a,op_l);
                    }
                }break;
            case 2:
                //Printing the current working directory
                printf("%s\n",getcwd(s,100));
                //Changing the current working directory to the previous directory
                chdir("..");
                //Printing the now current working directory
                printf("%s\n",getcwd(s,100));
                break;
            case 3:
                printf("\nvoce saiu do programa!\n");
                return 0;
        }
    }
	return 0;
}

// funções de integração com o usuário
int ls();  //done
int cd();   //done
int touch();
int mkdir();
int rm();
int rmdir();
int recover();
