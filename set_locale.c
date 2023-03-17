#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(int argc,char *argv[])
{

	if(argc != 2){
		printf("usage : %s local\n",argv[0]);
		exit(0);
	}
	if(strcmp(argv[1],"ja") == 0){
		system("cp po/ja.mo /usr/share/locale/ja/LC_MESSAGES/IHAPHONE.mo");
	}else if(strcmp(argv[1],"hr") == 0){
		system("cp po/hr.mo /usr/share/locale/ja/LC_MESSAGES/IHAPHONE.mo");
	}else if(strcmp(argv[1],"us") == 0){
		system("rm /usr/share/locale/ja/LC_MESSAGES/IHAPHONE.mo");
	}else{
		printf("locale %s not support\n",argv[1]);
	}

	return 0;
}
