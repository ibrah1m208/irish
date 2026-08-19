#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    /* TODO: Implement loop */
    printf("initiate C shell\n\n\n");
    
    /* Temporary: Show current user and directory */
    struct passwd *pw = getpwuid(getuid()); 
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        return 1;
    }
    
    printf("User: %s, current dir: %s\n", pw ? pw->pw_name : "unknown", cwd);
    printf("Press to Continue...\n");
    
    return 0;
}