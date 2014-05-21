MoLy
====

MoLy is a smart and modular monitoring layer for Software Defined Networks.

The three basic entities in the architecture are:
- DPI Controller - a management application that receives rule sets from middleboxes and distribute them to DPI service instances.
- DPI Service Instance - scans incoming traffic with rule sets assigned by the controller.
- Middlebox - any application that traditionally performs DPI, and now receives DPI results along with the packets themselves.

This project is now under research and initial development.
For further information please contact Yotam at yotamhc (at) cs.huji.ac.il
