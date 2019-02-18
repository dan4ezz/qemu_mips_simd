package com.berezka

import java.io.PrintWriter

import scala.io.Source

object Main extends App {
  def loadKeysFromFile(path: String): List[String] = {
    Source.fromFile(path).getLines().toList
  }

  def saveToFile(keys: List[String])(implicit pw: PrintWriter): Unit = {
    pw.write(keys.toString().drop(5).dropRight(1).replaceAll(",",""))
    pw.write(" ")
    pw.write("\n")
  }


  override def main(args: Array[String]): Unit = {
    implicit val pw: PrintWriter = new PrintWriter(Option(args(1)).getOrElse("result.txt"))

    val keys: List[String] = loadKeysFromFile(Option(args(0)).getOrElse("keys.txt"))

    println((4 to 7).flatMap(keys.combinations).size)

    (4 to 7).flatMap(keys.combinations).foreach(saveToFile)

    pw.close()
  }
}
