from behave import *

from steps.utils.BaseNtrip1Emitter import BaseNtrip1Emitter
from steps.utils.ntrip1Client import Ntrip1Client

use_step_matcher("re")

def emit_message(context):
    for base in context.bases:
        try:
            base.send_message()
        except Exception as e:
            print(e)



@given("the base (?P<base>.*) is emitting(?: with end of line (?P<eol>.*))?")
def step_impl(context, base, eol):
    """
    :type context: behave.runner.Context
    :type base: str
    :type eol: str
    """
    eol = "\n" if eol == "LF" else "\r\n"
    password = f"{base}pwd"
    if not hasattr(context, "bases"):
        context.bases = []
    context.bases.append(BaseNtrip1Emitter(base, password, eol))

@when('client connect to the base (?P<mount>.*)(?: with end of line (?P<eol>.*))?')
def step_impl(context, mount, eol):
    """
    :type context: behave.runner.Context
    :type base: str
    :type eol: str
    """
    eol = "\n" if eol == "LF" else "\r\n"
    context.client = Ntrip1Client(mount, "", "", eol)
    emit_message(context)


@then('client should have received a message from base (?P<mount>.*) in the last message')
def step_impl(context, mount):
    """
    :type context: behave.runner.Context
    """
    lines = context.client.get_line_received()
    last_line = lines[-1] if lines else ""
    assert last_line == f"base ~{mount}~", f"Expected 'base ~{mount}~' but got '{last_line}'"

