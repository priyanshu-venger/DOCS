
#include"REPL.h"
REPL r;

int main(){
    while(1){
        cout<<"\nEnter your choice:\n1:SET,2:GET,3:DELETE,4:EXIT\n";
        int choice;
        cin>>choice;
        if(choice>4 || choice<1) cout<<"Not in range\n";
        else{
            string key,value;
            if(choice==1) {
                cout<<"Enter key and value:";
                cin>>key>>value;
                if(r.SET(key,value)) cout<<"Successful\n";
                else cout<<"Failed\n";
            }
            else if(choice==2){
                cout<<"Enter key:";
                cin>>key;
                if(r.GET(key,value)) cout<<value<<"\n";
                else{
                    cout<<"Key not found\n";
                }
            }
            else if(choice==3){
                cout<<"Enter key:";
                cin>>key;
                if(r.DELETE(key)) cout<<"Successful\n";
                else cout<<"Failed\n";
            }
            else break;

        }
    }
    return 0;
}