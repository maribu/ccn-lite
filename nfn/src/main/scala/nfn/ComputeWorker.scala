package nfn

import akka.actor.{Actor, ActorRef}
import akka.event.Logging
import ccn.ccnlite.CCNLite
import ccn.packet.{CCNName, Interest, Content}
import scala.concurrent.Future
import scala.concurrent.ExecutionContext.Implicits.global
import nfn.service.{NFNValue, NFNService, CallableNFNService}
import scala.util.{Failure, Success}
import myutil.IOHelper
import ComputeWorker._

object ComputeWorker {
  case class Callable(callable: CallableNFNService)
  case class Exit()
}

/**
 *
 */
case class ComputeWorker(ccnServer: ActorRef) extends Actor {


  val logger = Logging(context.system, this)
  val ccnIf = CCNLite

  var maybeFutCallable: Option[Future[CallableNFNService]] = None

  def receivedContent(content: Content) = {
    // Received content from request (sendrcv)
    logger.error(s"ComputeWorker received content, discarding it because it does not know what to do with it")
  }

  // Received compute request
  // Make sure it actually is a compute request and forward to the handle method
  def receivedThunkRequest(computeName: CCNName, requestor: ActorRef) = {
    if(computeName.isCompute && computeName.isNFN && computeName.isThunk) {
      logger.debug(s"Received thunk request: $computeName")
      val computeCmps = computeName.withoutCompute.withoutThunk.withoutNFN
      handleThunkRequest(computeCmps, computeName, requestor)
    } else {
      logger.error(s"Dropping compute interest $computeName, because it does not begin with ${CCNName.computeKeyword}, end with ${CCNName.nfnKeyword} or is not a thunk, therefore is not a valid compute interest")
    }
  }


  /*
   * Parses the compute request and instantiates a callable service.
   * On success, sends a thunk back if required, executes the services and sends the result back.
   */
  def handleThunkRequest(computeName: CCNName, originalName: CCNName, requestor: ActorRef) = {
    logger.debug(s"Handling compute request for name: $computeName")
    assert(computeName.cmps.size == 1, "Compute cmps at this moment should only have one component")
    val futCallableServ: Future[CallableNFNService] = NFNService.parseAndFindFromName(computeName.cmps.head, ccnServer)

    // send back thunk content when callable service is creating (means everything was available)
    futCallableServ onSuccess {
      case callableServ => {
          // TODO: No default value for network
          requestor ! Content(originalName, callableServ.executionTimeEstimate.fold("")(_.toString).getBytes)
      }
    }
    maybeFutCallable = Some(futCallableServ)

  }

  override def receive: Actor.Receive = {
    case ComputeServer.Thunk(name) => {
      receivedThunkRequest(name, sender)
    }
    case ComputeServer.Compute(name) => {
      maybeFutCallable match {
        case Some(futCallable) => {
          futCallable onComplete {
            case Success(callable) => {
              val result: NFNValue = callable.exec
              val content = Content(name.withoutThunkAndIsThunk._1, result.toStringRepresentation.getBytes)
              logger.info(s"Finished computation, result: $content")
              sender ! content
            }
            case Failure(e) => {
              logger.error(e, "There was an error when creating the callable service")
            }
          }
        }
        case None => logger.error("Compute message was sent before thunk, which means that the service to execute has not yet been created")
      }
    }
    case Exit() => context.stop(self)
  }
}
