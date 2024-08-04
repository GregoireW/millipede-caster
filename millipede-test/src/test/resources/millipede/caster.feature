Feature: Is the caster work correctly?
  Check the caster emits the correct message

  Scenario: Check standard base
    Given The base NORTH is emitting
    When client connect to the base "NORTH"
    Then client should have received a message from base "NORTH" in the last message

  Scenario: Check base sending LF
    Given The base NORTH is emitting with end of line LF
    When client connect to the base "NORTH"
    Then client should have received a message from base "NORTH" in the last message

  #Scenario: Check client sending LF
  #  Given The base "NORTH" is emitting
  #  When client connect to the base "NORTH" with end of line "LF"
  #  Then client should have received a message from base "NORTH" in the last message
