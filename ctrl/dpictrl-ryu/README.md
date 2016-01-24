                                                
                        DPI Controller Demo - User Manual
                        ---------------------------------
                                                         
Background:
-----------
This demo consists of several components:
    - SDN controller (RYU)
    - TSA Backend
    - TSA Frontend
    - DPI Controller
    - DPI Instances
    - Middleboxes
    - Packet Generator
    
This program separates the data and the control planes. The SDN controller (RYU)
and the TSA are not operated from hosts, but on the outside server. However, 
the other components are operated from network host.

For further information regarding the demo please contact Tal at 
tal.orenstein (at) cs.huji.ac.il


Assumptions:
------------
    - The existence of appropriate tsa config file.
    
    - there are no cycles in the installed rules for the policy chain. 
      Let's say that we add the following policy chain by typing the command:
      addPolicyChain h1,m3,m5 {tp_dst=4545, nw_src=10.0.0.10}
      
      Then the next rules will be installed:
        - The first rule is installed in the switch which connected to the host
          that sends the packet (h1):
            - match fileds:  tp_dst=4545, nw_src=10.0.0.10, in_port=X
            - actions: push_vlan_vid, output_port
        
        - The last rule is installed in the switch which connected to the last
          middlebox (i.e right before 'm5' receives the packet) as follow:
            - match fields: in_port=X, dl_vlan=Y
            - actions: pop_vlan, output_port
              Note that by popping the vlan vid from the packet, we leave the
              responsibility of routing the packet to its destination to the STP
              of layer 2.
                          
        - The rest of the rules are installed as follow:
            - match fields: in_port=X, dl_vlan=Y
            - actions: output_port
      
      Thus, it is forbidden to add policy chain that cause the tsa to install 
      two rules on the same switch with the same match fields with different
      out_port actions. A possible solution for this issue is to devide the policy
      chain to several sub-chains, each with different vlan vid. Because the number
      of vlan vids are limited by 4094 (0 and 4095 are reserved), it is not a 
      good idea to allocate different vlan vid for each flow between two 
      middleboxes (for example, vlan_vid 1 for route between h1 to m3 and 
      vlan_vid 2 for route between m3 to m5).
      Therefore, a smarter mechanism is required (for example, a mechanism that 
      tries to maximize the number of paths with the same vlan vid).
      
      A possible workaround that prevents cycles is:
        - define the sender packet host to be h1.
        - define the DPI instances on hosts h-2,...,h-x.
        - define the middleboxes on hosts h-[x+1],...
      and define the middleboxes in an ascending order (the sorting of the middlebox
      can be done in the tsa before installing the rules, but in this project
      I stay strict to the user order).
    
    

A procedure to run the demo on mininet:  
---------------------------------------  
    
    - Before we start, you have to change the file extension 
      of ./DPIController/lib/java-json.jara to be 'jar' (instead of 'jara').
    
    - connect with ssh or other way to the server in which the ryu controller
      is installed, and open 4 terminals:
        - From the first terminal, run the SDN controller:        
            - get into ~/ryu dir (probably: cd ryu).
            - type the following line:
              sudo ryu-manager --observe-links ryu/app/simple_switch_stp.py ryu/app/rest_topology.py ryu/app/ofctl_rest.py
        
        - From the second terminal, run the mininet (attached Fat Tree topology):
            - get into ~/ryu/dpiControllerDemo
            - type the following line:
              sudo mn --custom ./ft-topo.py --topo FatTree,4,6,2 --mac --controller remote
              
            - on the mininet terminal, type the following lines:
                - py net.addLink(c0,h7)
                - h7 ifconfig h7-eth1 10.100
                - c0 ifconfig c0-eth0 10.101
                - h7 route add -host 10.0.0.101 dev h7-eth1
                
            - run the 'pingall' command until you receive a 100% success.
            - open a terminal for host h7 (xterm h7) - the DPI controller:
                - type the following lines:
                    - cd DPIController
                    - javac -cp ./lib/java-json.jar *.java
                    - cd ..
                    - java -cp DPIController:DPIController/lib/*: DPIController.Run [debug]
                      Note that the debug mode is optional
            
            - open terminals for the middleboxes that wants to be registered for DPI service.
              For each middlebox, type the following line:
              python middlebox.py <dpiController Ip Address> <dpiController server port> [match rules file path]
              Note: the match rules filepath is optioanl.
              Note: by typing: python middlebox.py auto     
              you run the middlebox automatically with:
                - dpi controller ip = '10.0.0.7'
                - dpi controller lisening port = 9091
                - match rules file path = 'SnortPatternsPartial.json'
              
            - open terminals for the dpi instances. For each dpi instance type:
                - cd DPIInstance/build
                - make
                - ./main name=hX autoft
                  Note: you should insert the name of the dpi instance as shown in
                  the (mininet) network (for example, name=h5 if host 5 will run DPI
                  instance).
                  Note: autoft is an automatic mode for fat tree topology.
            
            - open terminal for the packet genarator on h1 (the attached tsa 
              config file assume that h1 is the packet genarator host), and type:
              python packetsGenerator.py
              
        - From the third terminal, run the tsa backend. type:
            - cd ~/ryu/dpiControllerDemo/tsa
            - python tsaBE.py 10.0.0.100 9091 [debug]
            
        - From the fourth terminal, run the tsa frontend. type:
            - cd ~/ryu/dpiControllerDemo/tsa
            - python tsaFE.py [debug]
            
            

                                The Components:
                                ---------------

DPI Controller:
---------------
The DPI controller listens on '127.0.0.1', port 9091. This component doesn't 
include a command line. A detailed documentation is available in the file itself.
It supports the following commands:
    From DPI Instance:
    - register dpi instance (expects to receive the dpi instance name and ip as arguments):
        - if the dpi is already registered, the DPI controller sends all the match
          rules the DPI instance should scan.
          
    - remove dpi instance (expects to receive the dpi instance name as an argument):
        - on removal of DPI instance, the DPI controller adds automatically all 
          the policies chains handled by the this DPI instance to other 
          registered DPI instances.
        - In addition, it informs the DPI instances about the new match rules they
          should scan.
    
    From Middlebox:
    - register middlebox (expects to receive the middlebox name, its match rules 
      and two boolean arguments- isStealth and isFlow).
      
    - unregister middlebox (expects to receive the middlebox name as an argument):
        - during the removal of the middlebox, the DPI controller informs the 
          relevant DPI instances about the change.
          
    - set match rules (expects to receive the middlebox name and its new match rules):
        - As part of execution this command, the DPI controller informs the relevant 
          DPI instances about the change.
          
    
    From TSA:
    - add policy chain (expects to receive the policychain of middleboxes 
      [without the sender host] and a policychain id [pcid]):
        - As part of executing this command, the DPI controller looks for the 
          DPI instance that handles the fewest number of middleboxes, and 
          allocates it to scan this middlebox.
          
        - In addition, it informs the selected DPI instance about the new match
          rules it should scan.
        
    - remove policy chain (expects to receive the policychain of middleboxes 
      [without the sender host] and a policychain id [pcid]):
        - As part of executing this command, the DPI controller informs the DPI
          instance which scan this policy chain about the change.
        
    - For debug use:
        - print dpi controller status:
            - It prints the active middleboxes and dpi instances. In addition, it
              prints a matrix in which every column represents a DPI instance and
              every row (except the first row) represents a middlebox.
              
        - print dpi controller full status:
            - Beside the prints of the command above, it prints also the full 
              match rules of the middleboxes.



TSA Backend:
------------
The TSA backend listens on '10.0.0.101', port 9093. A detailed documentation 
is available in the file itself.
First, it builds the network's switches graph based on tsaConfigFile.txt and
rest API's of RYU SDN controller.

Regarding the configuration file:
    the configuration file is named 'tsaConfigFile.txt' and should be located on 
    'dpiControllerDemo' directory.
    The format of the configuration file is:
        - Each middlebox in the network should has a different line in the following format:
          <host_id> <switch_name_that_the_middlebox_is_connected_to> <host_mac_address>
          For example: for host h2, the line should look like:
          2 s11 00:00:00:00:00:02
        - An empty line
        - Each sender host in the network should has a different line in the following format:
          <host_id> <switch_name_that_the_host_is_connected_to> <host_mac_address>
          For example: for host h1, the line should look like:
          1 s11 00:00:00:00:00:01
        - In addition, you are allowed to add comments. A comment should be in 
          new line and start with '#' sign.

Regarding the network's switches graph:
    Each node is represented by a key-value mapping, such that the key is the 
    switch dpid and the value is a map
    with 2 keys: {'neighbors': [], 'isVisited': False}
    - 'neighbors': a list of all the neighbor's dpid and the port to reach them 
                   (for example, if s1 is connected to s1 via 
                   port 3 [i.e., (s1,3) -> s2], the neighbors list of s1 will 
                   contain this data: [3, s2 dpid]
    - 'isVisited': a boolean flag for finding a path in the graph.
    
    Each edge is represented by a key-value mapping. the key is a tuple of size 2 with the two nodes, and the
    value is always empty.
    
    Note that the edges are bidirectional.
    

The TSA BE supports the following commands:
    From TSA Frontend:
    - add policy chain (expects to receive the middleboxes policychain
      [starts with the sender host, but without the DPI instance], and the match
      fields for this policy chain):
        - As part of executing this command, It creates policy chain id (pcid)
          and sends the command 'add policy chain' to the DPI controller with the
          created pcid and the policychain.
          
        - Each policy chain receives vlan id.
        
        - A description for the installed rules are written already as part
          of the second assumption.
        
    - remove policy chain (expects to receive the policychain of middleboxes 
      [starts with the sender host, but without the DPI instance] and the match
      fields for this policy chain):
        - As part of executing this command, the TSA BE informs the DPI controller 
          about the change.
          
        - Then, it uninstalls the rules for this policy chain.
        
        - Important note: in case that a middlebox has unregistered, you should
          write the policy chain without the removed middlebox (since it's not 
          part of the policy chain when we want to remove the policy chain).
        
    - exit:
        - exit the TSA BE.
        
            
    - For debug use:
        - print dpi controller status:
            - It sends the command to the DPI controller.
              
        - print dpi controller full status:
            - It sends the command to the DPI controller.
            
            
    From DPI Controller:
    - unregister middlebox (expects to receive the unregistered middlebox name):
        - As part of executing this command, the TSA BE uninstalls the rules for
          the old policy chain, and installs the rules for the new policy chain
          (without the unregistered middlebox).
          Note that a partial removal of flows is problematic since the same
          flow may be used later during the policy chain.    
          
    - replace dpi instance (expects to receive the new DPI instance name and the
      pcid of the policy chain):
        - As part of executing this command, the TSA BE uninstalls the rules for
          the old policy chain, and installs the rules for the new policy chain 
          (with the new DPI instance).
          Note that a partial removal of flows is problematic since the same
          flow may be used later during the policy chain.
          



TSA Frontend:
-------------
The TSA frontend allows the user to type the following commands and send
them to the TSA backend:
    - add policy chain (for example: addpolicychain h1,m3,m5 {tp_dst=4545, nw_src=10.0.0.10})
    - remove policy chain (for example: removepolicychain h1,m3,m5 {tp_dst=4545, nw_src=10.0.0.10})
    - For debug use:
        - print DPI controller status
        - print DPI controller full status

Some important notes:
    - A policy chain must start with the sender host (packet generator).
    - the packet generator is not a middlebox. Thus, it should be defined with
      the 'h' prefix (for example: h1).
    - the middleboxes in the policy chain should start with 'm' prefix.
    - the order of the match rules is unimportant. 





Middlebox:
----------
During the middlebox initialization it sends 'registerMiddlebox' command to the 
DPI controller. In addition, it sniffers for packets that are part of policy
chain (in order to send them back - and by that continue the chain).
  
The middlebox supports the following commands:
    - reload match rules (if the path is specified, it reloads the
      match rules from the file located in this path. Else, it reloads the file
      located in the same path as before):
        - After reloading the match rules, the middlebox sends the command
          'reload match rules' to the DPI controller with the updated match rules.
    
    - exit (unregister):
      This command sends 'unregister middlebox' command to the DPI controller 
      and then closes the program.
 
 
 
 
Packets Generator:
-----------------
The packets generator creates and sends packets in the network so we can test
the whole project.

The packet generator supports the following commands:
    - send {dl_dst=, dl_src=, nw_src=, nw_dst=, tp_src=, tp_dst=}:
      For example: send {dl_dst=00:00:00:00:00:03, tp_src=4545, nw_dst=10.0.0.3}
      Note that the default dl_dst is 00:00:00:00:00:07.
      
    - qsend <tp_dst>:
      For example: qsend 4545
      This command is designed for quick send to h9 in the SimpleTree topology
      in case that the match rule is defined according the destination port.
      
    - qsendf <tp_dst>:
      For example: qsendf 4545
      This command is designed for quick send to h9 in the Fat Tree topology
      in case that the match rule is defined according the destination port.