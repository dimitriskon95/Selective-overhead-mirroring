import java.util.ArrayList;

class apples{
    public static void main(String args[]) {
	ArrayList<Integer> arrList1 = new ArrayList<Integer>(5);
	ArrayList<Integer> arrList2 = new ArrayList<Integer>(5);

	arrList1.add(15);
	arrList1.add(20);
	arrList1.add(30);
	arrList1.add(40);
	
	arrList2.add(150);
	arrList2.add(200);
	arrList2.add(300);
	arrList2.add(400);

	for(Integer number: arrList1){
		System.out.println("Number = " + number);
	}
	
	int retval=arrList1.get(3);
	
	System.out.println("Retrieved element is = " + retval);	

	for(Integer number: arrList1){
			System.out.println("Number = " + number);
		}
	arrList1.addAll(arrList2);


	for(Integer number: arrList1){
			System.out.println("Number = " + number);
		}
    }
}
