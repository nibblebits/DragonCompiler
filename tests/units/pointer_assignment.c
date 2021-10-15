
int main()
{
  int x;
  x = 20;
  int* ptr;
  ptr = &x;
  *ptr = 30;
  
  return x;
}