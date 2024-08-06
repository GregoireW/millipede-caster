

def after_scenario(context, scenario):
    if hasattr(context, "bases"):
        for base in context.bases:
            base.stop()
        context.bases=[]
    if hasattr(context, "client"):
        if context.client:
            context.client.stop()
            context.client = None

