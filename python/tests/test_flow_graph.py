#!/usr/bin/env python3

from transitions.extensions import HierarchicalGraphMachine as Machine
#from transitions.extensions import GraphMachine as Machine

from zio.flow.sm import states, transitions

machine = Machine(states=states, transitions=transitions,
                  initial='IDLE',
                  use_pygraphviz=False,
                  show_conditions=True,
                  show_auto_transitions=False)

pdf='test_flow_graph.pdf'
machine.get_graph().draw(pdf,prog='dot')
print(pdf)
