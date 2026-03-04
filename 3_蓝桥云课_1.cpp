#include <iostream>
#include <string>
using namespace std;

int dp[10];

int main()
{
  int n;
  cin>>n;
  string s;
  int m=0;
  for(int i=0;i<n;++i){
    cin>>s;
    int x=s[0]-'0',y=s[s.size()-1]-'0';
    dp[y]=max(dp[x]+1,dp[y]);
    m=max(m,dp[y]);
  }
  cout<<n-m<<endl;
  return 0;
}