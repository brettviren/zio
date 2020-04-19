
from transitions.extensions import HierarchicalMachine as Machine

states = ['standing', 'walking',
          {'name': 'caffeinated', 'initial': 'dithering',
           'children': ['dithering', 'running']}]
transitions = [
  ['walk', 'standing', 'walking'],
  ['stop', 'walking', 'standing'],
  # this transition will end in 'caffeinated_dithering'...
  ['drink', '*', 'caffeinated'],
  # ... that is why we do not need do specify 'caffeinated' here anymore
  ['walk',  'caffeinated_dithering', 'caffeinated_running'],
  ['relax', 'caffeinated', 'standing']
]

# states = ['standing', 'walking',
#           {'name': 'caffeinated', 'children':['dithering', 'running']}]
# transitions = [
#   ['walk', 'standing', 'walking'],
#   ['stop', 'walking', 'standing'],
#   ['drink', '*', 'caffeinated'],
#   ['walk', ['caffeinated', 'caffeinated_dithering'], 'caffeinated_running'],
#   ['relax', 'caffeinated', 'standing']
# ]

machine = Machine(states=states, transitions=transitions, initial='standing', ignore_invalid_triggers=True)

machine.walk() # Walking now
machine.stop() # let's stop for a moment
machine.drink() # coffee time
print(machine.state)

machine.walk() # we have to go faster
print(machine.state)

machine.stop() # can't stop moving!
print(machine.state)

machine.relax() # leave nested state
print(machine.state) # phew, what a ride

# machine.on_enter_caffeinated_running('callback_method')
