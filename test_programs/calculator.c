#include <stdio.h>
int main() {
  char op;
  int first, second;
  printf("Enter an operator (+, -, *, /): ");
  scanf("%c", &op);
  printf("Enter two operands: ");
  scanf("%i %i", &first, &second);

  switch (op) {
    case '+':
      printf("%i+ %i = %i", first, second, first + second);
      break;
    case '-':
      printf("%i- %i = %i", first, second, first - second);
      break;
    case '*':
      printf("%i*%i = %i", first, second, first * second);
      break;
    case '/':
      printf("%i/ %i = %i", first, second, first / second);
      break;
    // operator doesn't match any case constant
    default:
      printf("Error! operator is not correct");
  }

  return 0;
}