#include "m_malloc.h"
#include <assert.h>


int main() {

    #define malloc(x) m_malloc(x)
    #define free(x) m_free(x)


    // test 1 ~ test 5: https://github.com/araohatkokate/malloc.git
    
    // test 1
    #ifdef test1
    {
        char * ptr = ( char * ) malloc ( 65535 );
        free( ptr ); 
    }
    #endif

    // test 2
    #ifdef test2
    {
        char * ptr = ( char * ) malloc ( 65535 );
        char * ptr_array[1024];

        int i;
        for ( i = 0; i < 1024; i++ )
        {
            ptr_array[i] = ( char * ) malloc ( 1024 ); 
            
            ptr_array[i] = ptr_array[i];
        }

        free( ptr );

        for ( i = 0; i < 1024; i++ )
        {
            free( ptr_array[i] );
        }

        ptr = ( char * ) malloc ( 65535 );
        free( ptr ); 
    }
    #endif

    // test 3 
    #ifdef test3
    {
        char * ptr1 = ( char * ) malloc ( 1200 );
        char * ptr2 = ( char * ) malloc ( 1200 );

        free( ptr1 );
        free( ptr2 );

        char * ptr3 = ( char * ) malloc ( 2048 );

        free( ptr3 );
    }
    #endif

    // test 4
    #ifdef test4
    {

        char * ptr1 = ( char * ) malloc( 2048 );
        free( ptr1 );
        char * ptr2 = ( char * ) malloc( 1024 );
        free( ptr2 );
    }
    #endif

    #ifdef test5
    {
        char * ptr1 = ( char * ) malloc ( 65535 );
        char * buffer1 = ( char * ) malloc( 1 );
        char * ptr4 = ( char * ) malloc ( 65 );
        char * buffer2 = ( char * ) malloc( 1 );
        char * ptr2 = ( char * ) malloc ( 6000 );

        free( ptr1 ); 
        free( ptr2 ); 

        buffer1 = buffer1;
        buffer2 = buffer2;
        ptr4 = ptr4;

        char * ptr3 = ( char * ) malloc ( 1000 );
        free (ptr3);

        free(buffer1);
        free(buffer2);
        free(ptr4);
    }
    #endif

    return 0;

}